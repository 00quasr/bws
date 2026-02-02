#include "infrastructure/network/PortScanner.hpp"

#include "core/types/PortScanResult.hpp"

#include <spdlog/spdlog.h>

namespace netpulse::infra {

PortScanner::PortScanner(AsioContext& context) : context_(context) {}

PortScanner::~PortScanner() {
    cancel();
}

void PortScanner::scanAsync(const core::PortScanConfig& config, ResultCallback onResult,
                            ProgressCallback onProgress, CompletionCallback onComplete) {
    if (scanning_.exchange(true)) {
        spdlog::warn("Scan already in progress");
        return;
    }

    cancelled_ = false;
    auto ports = config.getPortsToScan();

    spdlog::info("Starting port scan of {} on {} ports", config.targetAddress, ports.size());

    auto progress = std::make_shared<core::PortScanProgress>();
    progress->totalPorts = static_cast<int>(ports.size());

    auto results = std::make_shared<std::vector<core::PortScanResult>>();
    auto completedCount = std::make_shared<std::atomic<int>>(0);
    auto openCount = std::make_shared<std::atomic<int>>(0);
    auto resultsMutex = std::make_shared<std::mutex>();

    semaphore_ = std::make_unique<std::counting_semaphore<>>(config.maxConcurrency);

    for (uint16_t port : ports) {
        if (cancelled_) {
            break;
        }

        semaphore_->acquire();

        // Create shared state for this port scan
        auto scanState = std::make_shared<struct ScanState>();
        scanState->socket = std::make_shared<asio::ip::tcp::socket>(context_.getContext());
        scanState->timer = std::make_shared<asio::steady_timer>(context_.getContext());
        scanState->result.targetAddress = config.targetAddress;
        scanState->result.port = port;
        scanState->result.scanTimestamp = std::chrono::system_clock::now();

        if (cancelled_) {
            semaphore_->release();
            break;
        }

        try {
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(config.targetAddress), port);

            // Start timeout timer
            scanState->timer->expires_after(config.timeout);
            scanState->timer->async_wait(
                [this, scanState, onResult, onProgress, onComplete, progress, results,
                 completedCount, openCount, resultsMutex,
                 totalPorts = ports.size()](const asio::error_code& ec) {
                    if (ec || scanState->completed.exchange(true)) {
                        return; // Timer cancelled or already completed
                    }

                    // Timeout occurred
                    try {
                        scanState->socket->close();
                    } catch (...) {
                    }
                    scanState->result.state = core::PortState::Filtered;

                    finishPortScan(scanState->result, onResult, onProgress, onComplete, progress,
                                   results, completedCount, openCount, resultsMutex, totalPorts);
                });

            // Start async connect
            scanState->socket->async_connect(
                endpoint,
                [this, scanState, onResult, onProgress, onComplete, progress, results,
                 completedCount, openCount, resultsMutex,
                 totalPorts = ports.size()](const asio::error_code& ec) {
                    if (scanState->completed.exchange(true)) {
                        return; // Already completed (timeout)
                    }

                    // Cancel the timer
                    scanState->timer->cancel();

                    if (!ec) {
                        scanState->result.state = core::PortState::Open;
                        scanState->result.serviceName =
                            core::ServiceDetector::detectService(scanState->result.port);
                        (*openCount)++;
                    } else {
                        scanState->result.state = core::PortState::Closed;
                    }

                    try {
                        scanState->socket->close();
                    } catch (...) {
                    }

                    finishPortScan(scanState->result, onResult, onProgress, onComplete, progress,
                                   results, completedCount, openCount, resultsMutex, totalPorts);
                });

        } catch (const std::exception& e) {
            // Invalid address or other error
            spdlog::debug("Port scan error for {}:{} - {}", config.targetAddress, port, e.what());
            scanState->result.state = core::PortState::Closed;
            finishPortScan(scanState->result, onResult, onProgress, onComplete, progress, results,
                           completedCount, openCount, resultsMutex, ports.size());
        }
    }
}

void PortScanner::finishPortScan(const core::PortScanResult& result, ResultCallback onResult,
                                  ProgressCallback onProgress, CompletionCallback onComplete,
                                  std::shared_ptr<core::PortScanProgress> progress,
                                  std::shared_ptr<std::vector<core::PortScanResult>> results,
                                  std::shared_ptr<std::atomic<int>> completedCount,
                                  std::shared_ptr<std::atomic<int>> openCount,
                                  std::shared_ptr<std::mutex> resultsMutex, size_t totalPorts) {
    // Store result
    if (result.state == core::PortState::Open) {
        std::lock_guard lock(*resultsMutex);
        results->push_back(result);
    }

    // Report individual result
    if (onResult && result.state == core::PortState::Open) {
        onResult(result);
    }

    // Update progress
    int completed = ++(*completedCount);
    if (onProgress) {
        progress->scannedPorts = completed;
        progress->openPorts = openCount->load();
        progress->cancelled = cancelled_.load();
        onProgress(*progress);
    }

    semaphore_->release();

    // Check if scan is complete
    if (completed == static_cast<int>(totalPorts)) {
        scanning_ = false;
        spdlog::info("Port scan complete: {} open ports found", openCount->load());
        if (onComplete) {
            std::lock_guard lock(*resultsMutex);
            onComplete(*results);
        }
    }
}

void PortScanner::cancel() {
    if (scanning_) {
        spdlog::info("Cancelling port scan");
        cancelled_ = true;
    }
}

bool PortScanner::isScanning() const {
    return scanning_.load();
}

} // namespace netpulse::infra
