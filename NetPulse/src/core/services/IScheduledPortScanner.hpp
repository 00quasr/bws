#pragma once

#include "core/types/ScheduledPortScan.hpp"

#include <functional>
#include <vector>

namespace netpulse::core {

class IScheduledPortScanner {
public:
    using ScanCompleteCallback = std::function<void(int64_t scheduleId, const std::vector<PortScanResult>&)>;
    using DiffCallback = std::function<void(int64_t scheduleId, const PortScanDiff&)>;

    virtual ~IScheduledPortScanner() = default;

    virtual void addSchedule(const ScheduledScanConfig& config) = 0;
    virtual void updateSchedule(const ScheduledScanConfig& config) = 0;
    virtual void removeSchedule(int64_t scheduleId) = 0;
    virtual void enableSchedule(int64_t scheduleId, bool enabled) = 0;

    virtual std::vector<ScheduledScanConfig> getSchedules() const = 0;
    virtual std::optional<ScheduledScanConfig> getSchedule(int64_t scheduleId) const = 0;

    virtual void runNow(int64_t scheduleId) = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    virtual void setScanCompleteCallback(ScanCompleteCallback callback) = 0;
    virtual void setDiffCallback(DiffCallback callback) = 0;

    virtual PortScanDiff computeDiff(const std::string& targetAddress,
                                      const std::vector<PortScanResult>& previous,
                                      const std::vector<PortScanResult>& current) = 0;
};

} // namespace netpulse::core
