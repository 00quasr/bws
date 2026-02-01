/**
 * @file IPingService.hpp
 * @brief Interface for the ICMP ping service.
 *
 * This file defines the abstract interface for performing ICMP ping
 * operations and monitoring hosts for availability.
 */

#pragma once

#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"

#include <functional>
#include <future>
#include <memory>

namespace netpulse::core {

/**
 * @brief Interface for ICMP ping service.
 *
 * Provides methods for performing one-off ping operations and continuous
 * monitoring of hosts.
 *
 * @note On Linux, ICMP ping requires CAP_NET_RAW capability or root privileges.
 */
class IPingService {
public:
    /**
     * @brief Callback function type for ping result notifications.
     * @param result The result of the ping operation.
     */
    using PingCallback = std::function<void(const PingResult&)>;

    virtual ~IPingService() = default;

    /**
     * @brief Performs an asynchronous ping to the specified address.
     * @param address IP address or hostname to ping.
     * @param timeout Maximum time to wait for a response.
     * @return Future that will contain the ping result.
     */
    virtual std::future<PingResult> pingAsync(const std::string& address,
                                              std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Starts periodic monitoring of a host.
     * @param host The host configuration to monitor.
     * @param callback Function to call with each ping result.
     */
    virtual void startMonitoring(const Host& host, PingCallback callback) = 0;

    /**
     * @brief Stops monitoring a specific host.
     * @param hostId ID of the host to stop monitoring.
     */
    virtual void stopMonitoring(int64_t hostId) = 0;

    /**
     * @brief Stops monitoring all hosts.
     */
    virtual void stopAllMonitoring() = 0;

    /**
     * @brief Checks if a host is currently being monitored.
     * @param hostId ID of the host to check.
     * @return True if the host is being monitored.
     */
    virtual bool isMonitoring(int64_t hostId) const = 0;
};

} // namespace netpulse::core
