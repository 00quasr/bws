#pragma once

#include "core/services/IPortScanner.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <semaphore>

namespace netpulse::infra {

/**
 * @brief Asynchronous TCP port scanner for network reconnaissance.
 *
 * Performs TCP connect scans on specified port ranges with configurable
 * concurrency limits. Uses Asio for non-blocking I/O operations.
 * Implements the core::IPortScanner interface.
 */
class PortScanner : public core::IPortScanner {
public:
    /**
     * @brief Constructs a PortScanner with the given Asio context.
     * @param context Reference to the AsioContext for async operations.
     */
    explicit PortScanner(AsioContext& context);

    /**
     * @brief Destructor. Cancels any active scan and releases resources.
     */
    ~PortScanner() override;

    /**
     * @brief Starts an asynchronous port scan operation.
     * @param config Scan configuration (target, ports, timeout, concurrency).
     * @param onResult Callback invoked for each port scan result.
     * @param onProgress Callback invoked to report scan progress (0.0-1.0).
     * @param onComplete Callback invoked when the scan completes or is cancelled.
     */
    void scanAsync(const core::PortScanConfig& config, ResultCallback onResult,
                   ProgressCallback onProgress, CompletionCallback onComplete) override;

    /**
     * @brief Cancels the currently running scan.
     *
     * Sets the cancelled flag and stops new port scans from starting.
     * In-progress connections will complete or timeout.
     */
    void cancel() override;

    /**
     * @brief Checks if a scan is currently in progress.
     * @return True if scanning, false otherwise.
     */
    bool isScanning() const override;

private:
    struct ScanState {
        std::shared_ptr<asio::ip::tcp::socket> socket;
        std::shared_ptr<asio::steady_timer> timer;
        core::PortScanResult result;
        std::atomic<bool> completed{false};
    };

    void finishPortScan(const core::PortScanResult& result, ResultCallback onResult,
                        ProgressCallback onProgress, CompletionCallback onComplete,
                        std::shared_ptr<core::PortScanProgress> progress,
                        std::shared_ptr<std::vector<core::PortScanResult>> results,
                        std::shared_ptr<std::atomic<int>> completedCount,
                        std::shared_ptr<std::atomic<int>> openCount,
                        std::shared_ptr<std::mutex> resultsMutex, size_t totalPorts);

    AsioContext& context_;
    std::atomic<bool> scanning_{false};
    std::atomic<bool> cancelled_{false};
    std::unique_ptr<std::counting_semaphore<>> semaphore_;
    mutable std::mutex mutex_;
};

} // namespace netpulse::infra
