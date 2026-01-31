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

        context_.post([this, address = config.targetAddress, port, timeout = config.timeout,
                       onResult, onProgress, onComplete, progress, results, completedCount,
                       openCount, resultsMutex, totalPorts = ports.size()]() {
            if (cancelled_) {
                semaphore_->release();
                return;
            }

            core::PortScanResult result;
            result.targetAddress = address;
            result.port = port;
            result.scanTimestamp = std::chrono::system_clock::now();

            try {
                asio::io_context& ctx = context_.getContext();
                asio::ip::tcp::socket socket(ctx);
                asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address), port);

                asio::steady_timer timer(ctx);
                timer.expires_after(timeout);

                std::atomic<bool> connected{false};
                std::atomic<bool> timedOut{false};

                socket.async_connect(endpoint, [&](const asio::error_code& ec) {
                    if (!timedOut) {
                        connected = !ec;
                        timer.cancel();
                    }
                });

                timer.async_wait([&](const asio::error_code& ec) {
                    if (!ec && !connected) {
                        timedOut = true;
                        socket.close();
                    }
                });

                // Run until both operations complete
                while (!connected && !timedOut) {
                    ctx.run_one();
                }

                if (connected) {
                    result.state = core::PortState::Open;
                    result.serviceName = core::ServiceDetector::detectService(port);
                    (*openCount)++;
                } else if (timedOut) {
                    result.state = core::PortState::Filtered;
                } else {
                    result.state = core::PortState::Closed;
                }

                socket.close();
            } catch (const std::exception& e) {
                result.state = core::PortState::Closed;
            }

            // Store result
            {
                std::lock_guard lock(*resultsMutex);
                if (result.state == core::PortState::Open) {
                    results->push_back(result);
                }
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
        });
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
