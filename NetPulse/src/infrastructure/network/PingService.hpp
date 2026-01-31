#pragma once

#include "core/services/IPingService.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace netpulse::infra {

class PingService : public core::IPingService {
public:
    explicit PingService(AsioContext& context);
    ~PingService() override;

    std::future<core::PingResult> pingAsync(const std::string& address,
                                            std::chrono::milliseconds timeout) override;

    void startMonitoring(const core::Host& host, PingCallback callback) override;
    void stopMonitoring(int64_t hostId) override;
    void stopAllMonitoring() override;

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
