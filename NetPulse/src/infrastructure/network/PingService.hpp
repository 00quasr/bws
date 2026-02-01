#pragma once

#include "core/services/IPingService.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace netpulse::infra {

/**
 * @brief ICMP ping service for network host reachability testing.
 *
 * Provides asynchronous ping operations and continuous host monitoring using
 * raw ICMP sockets. Implements the core::IPingService interface.
 *
 * @note On Linux, requires CAP_NET_RAW capability or root privileges.
 *       On macOS, uses SOCK_DGRAM for non-privileged ICMP.
 */
class PingService : public core::IPingService {
public:
    /**
     * @brief Constructs a PingService with the given Asio context.
     * @param context Reference to the AsioContext for async operations.
     */
    explicit PingService(AsioContext& context);

    /**
     * @brief Destructor. Stops all monitoring and releases resources.
     */
    ~PingService() override;

    /**
     * @brief Performs an asynchronous ping to the specified address.
     * @param address Target hostname or IP address to ping.
     * @param timeout Maximum time to wait for a response.
     * @return Future containing the PingResult with latency or error info.
     */
    std::future<core::PingResult> pingAsync(const std::string& address,
                                            std::chrono::milliseconds timeout) override;

    /**
     * @brief Starts continuous monitoring of a host with periodic pings.
     * @param host The host to monitor (includes ping interval settings).
     * @param callback Function called with each ping result.
     */
    void startMonitoring(const core::Host& host, PingCallback callback) override;

    /**
     * @brief Stops monitoring a specific host.
     * @param hostId Unique identifier of the host to stop monitoring.
     */
    void stopMonitoring(int64_t hostId) override;

    /**
     * @brief Stops monitoring all hosts.
     */
    void stopAllMonitoring() override;

    /**
     * @brief Checks if a host is currently being monitored.
     * @param hostId Unique identifier of the host.
     * @return True if the host is being monitored, false otherwise.
     */
    bool isMonitoring(int64_t hostId) const override;

private:
    struct MonitoredHost {
        core::Host host;
        PingCallback callback;
        std::shared_ptr<asio::steady_timer> timer;
        std::atomic<bool> active{true};
    };

    void scheduleNextPing(std::shared_ptr<MonitoredHost> monitored);
    core::PingResult performPing(const std::string& address, std::chrono::milliseconds timeout);

    // ICMP helpers
    static uint16_t calculateChecksum(const uint8_t* data, size_t length);
    static std::vector<uint8_t> buildIcmpEchoRequest(uint16_t identifier, uint16_t sequence);

    AsioContext& context_;
    std::map<int64_t, std::shared_ptr<MonitoredHost>> monitoredHosts_;
    mutable std::mutex mutex_;
    std::atomic<uint16_t> sequenceNumber_{0};
    uint16_t identifier_;
};

} // namespace netpulse::infra
