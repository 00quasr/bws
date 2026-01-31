#pragma once

#include "core/types/ScheduledPortScan.hpp"
#include "infrastructure/database/Database.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace netpulse::infra {

class ScheduledScanRepository {
public:
    explicit ScheduledScanRepository(std::shared_ptr<Database> db);

    int64_t insertSchedule(const core::ScheduledScanConfig& config);
    void updateSchedule(const core::ScheduledScanConfig& config);
    void removeSchedule(int64_t id);

    std::optional<core::ScheduledScanConfig> findById(int64_t id);
    std::vector<core::ScheduledScanConfig> findAll();
    std::vector<core::ScheduledScanConfig> findEnabled();

    void updateLastRunAt(int64_t id, std::chrono::system_clock::time_point time);
    void setEnabled(int64_t id, bool enabled);

    int64_t insertDiff(const core::PortScanDiff& diff, int64_t scheduleId);
    std::vector<core::PortScanDiff> getDiffs(int64_t scheduleId, int limit = 100);
    std::vector<core::PortScanDiff> getDiffsByAddress(const std::string& address, int limit = 100);
    void cleanupOldDiffs(std::chrono::hours maxAge);

    std::string exportDiffToJson(const core::PortScanDiff& diff);

private:
    std::string portRangeToString(core::PortRange range);
    core::PortRange portRangeFromString(const std::string& str);
    std::string portsToString(const std::vector<uint16_t>& ports);
    std::vector<uint16_t> portsFromString(const std::string& str);

    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
