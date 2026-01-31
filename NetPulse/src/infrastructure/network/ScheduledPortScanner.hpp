#pragma once

#include "core/services/IPortScanner.hpp"
#include "core/services/IScheduledPortScanner.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace netpulse::infra {

class ScheduledPortScanner : public core::IScheduledPortScanner {
public:
    ScheduledPortScanner(AsioContext& context, core::IPortScanner& portScanner);
    ~ScheduledPortScanner() override;

    void addSchedule(const core::ScheduledScanConfig& config) override;
    void updateSchedule(const core::ScheduledScanConfig& config) override;
    void removeSchedule(int64_t scheduleId) override;
    void enableSchedule(int64_t scheduleId, bool enabled) override;

    std::vector<core::ScheduledScanConfig> getSchedules() const override;
    std::optional<core::ScheduledScanConfig> getSchedule(int64_t scheduleId) const override;

    void runNow(int64_t scheduleId) override;

    void start() override;
    void stop() override;
    bool isRunning() const override;

    void setScanCompleteCallback(ScanCompleteCallback callback) override;
    void setDiffCallback(DiffCallback callback) override;

    core::PortScanDiff computeDiff(const std::string& targetAddress,
                                    const std::vector<core::PortScanResult>& previous,
                                    const std::vector<core::PortScanResult>& current) override;

    void setLastScanResults(int64_t scheduleId, std::vector<core::PortScanResult> results);
    std::vector<core::PortScanResult> getLastScanResults(int64_t scheduleId) const;

private:
    struct ScheduledItem {
        core::ScheduledScanConfig config;
        std::shared_ptr<asio::steady_timer> timer;
        std::vector<core::PortScanResult> lastResults;
        std::atomic<bool> active{true};
    };

    void scheduleNextScan(std::shared_ptr<ScheduledItem> item);
    void executeScan(std::shared_ptr<ScheduledItem> item);
    int64_t generateId();

    AsioContext& context_;
    core::IPortScanner& portScanner_;
    std::map<int64_t, std::shared_ptr<ScheduledItem>> schedules_;
    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> nextId_{1};

    ScanCompleteCallback scanCompleteCallback_;
    DiffCallback diffCallback_;
};

} // namespace netpulse::infra
