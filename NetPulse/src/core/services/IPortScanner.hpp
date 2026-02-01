/**
 * @file IPortScanner.hpp
 * @brief Interface for the port scanning service.
 *
 * This file defines the abstract interface for performing TCP port scans
 * and tracking scan progress.
 */

#pragma once

#include "core/types/PortScanResult.hpp"

#include <functional>
#include <future>
#include <vector>

namespace netpulse::core {

/**
 * @brief Progress information during a port scan operation.
 */
struct PortScanProgress {
    int totalPorts{0};    ///< Total number of ports to scan
    int scannedPorts{0};  ///< Number of ports scanned so far
    int openPorts{0};     ///< Number of open ports found
    bool cancelled{false}; ///< Whether the scan was cancelled

    /**
     * @brief Calculates the completion percentage.
     * @return Percentage of ports scanned (0-100).
     */
    [[nodiscard]] double percentComplete() const {
        return totalPorts > 0 ? (static_cast<double>(scannedPorts) / totalPorts) * 100.0 : 0.0;
    }
};

/**
 * @brief Interface for port scanning service.
 *
 * Provides methods for scanning TCP ports on remote hosts with progress
 * reporting and cancellation support.
 */
class IPortScanner {
public:
    /**
     * @brief Callback function type for progress updates.
     * @param progress Current scan progress information.
     */
    using ProgressCallback = std::function<void(const PortScanProgress&)>;

    /**
     * @brief Callback function type for individual port results.
     * @param result The scan result for a single port.
     */
    using ResultCallback = std::function<void(const PortScanResult&)>;

    /**
     * @brief Callback function type for scan completion.
     * @param results Vector of all scan results.
     */
    using CompletionCallback = std::function<void(const std::vector<PortScanResult>&)>;

    virtual ~IPortScanner() = default;

    /**
     * @brief Starts an asynchronous port scan.
     * @param config Configuration specifying target and ports to scan.
     * @param onResult Callback for each individual port result.
     * @param onProgress Callback for progress updates.
     * @param onComplete Callback when scan finishes with all results.
     */
    virtual void scanAsync(const PortScanConfig& config, ResultCallback onResult,
                           ProgressCallback onProgress, CompletionCallback onComplete) = 0;

    /**
     * @brief Cancels the current scan operation.
     */
    virtual void cancel() = 0;

    /**
     * @brief Checks if a scan is currently in progress.
     * @return True if a scan is running.
     */
    virtual bool isScanning() const = 0;
};

} // namespace netpulse::core
