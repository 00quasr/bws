#pragma once

#include "core/types/ScheduledPortScan.hpp"
#include "infrastructure/database/Database.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Repository for scheduled port scan configurations and diff history.
 *
 * Provides persistence operations for scheduled scan configurations
 * and tracks scan differences over time for change detection.
 */
class ScheduledScanRepository {
public:
    /**
     * @brief Constructs a ScheduledScanRepository with the given database.
     * @param db Shared pointer to the Database instance.
     */
    explicit ScheduledScanRepository(std::shared_ptr<Database> db);

    /**
     * @brief Inserts a new scheduled scan configuration.
     * @param config ScheduledScanConfig to store.
     * @return ID of the inserted schedule.
     */
    int64_t insertSchedule(const core::ScheduledScanConfig& config);

    /**
     * @brief Updates an existing scheduled scan configuration.
     * @param config ScheduledScanConfig with updated values (id must be set).
     */
    void updateSchedule(const core::ScheduledScanConfig& config);

    /**
     * @brief Removes a scheduled scan configuration.
     * @param id ID of the schedule to remove.
     */
    void removeSchedule(int64_t id);

    /**
     * @brief Finds a schedule by its ID.
     * @param id ID of the schedule to find.
     * @return ScheduledScanConfig if found, nullopt otherwise.
     */
    std::optional<core::ScheduledScanConfig> findById(int64_t id);

    /**
     * @brief Retrieves all scheduled scan configurations.
     * @return Vector of all ScheduledScanConfig entities.
     */
    std::vector<core::ScheduledScanConfig> findAll();

    /**
     * @brief Retrieves all enabled scheduled scan configurations.
     * @return Vector of enabled schedules.
     */
    std::vector<core::ScheduledScanConfig> findEnabled();

    /**
     * @brief Updates the last run timestamp for a schedule.
     * @param id ID of the schedule.
     * @param time Timestamp of the last run.
     */
    void updateLastRunAt(int64_t id, std::chrono::system_clock::time_point time);

    /**
     * @brief Enables or disables a schedule.
     * @param id ID of the schedule.
     * @param enabled True to enable, false to disable.
     */
    void setEnabled(int64_t id, bool enabled);

    /**
     * @brief Inserts a port scan diff record.
     * @param diff PortScanDiff to store.
     * @param scheduleId ID of the associated schedule.
     * @return ID of the inserted diff record.
     */
    int64_t insertDiff(const core::PortScanDiff& diff, int64_t scheduleId);

    /**
     * @brief Retrieves scan diffs for a schedule.
     * @param scheduleId ID of the schedule.
     * @param limit Maximum number of diffs to return.
     * @return Vector of PortScanDiff records.
     */
    std::vector<core::PortScanDiff> getDiffs(int64_t scheduleId, int limit = 100);

    /**
     * @brief Retrieves scan diffs for a target address.
     * @param address Target address to query.
     * @param limit Maximum number of diffs to return.
     * @return Vector of PortScanDiff records.
     */
    std::vector<core::PortScanDiff> getDiffsByAddress(const std::string& address, int limit = 100);

    /**
     * @brief Removes diff records older than the specified age.
     * @param maxAge Maximum age of records to keep.
     */
    void cleanupOldDiffs(std::chrono::hours maxAge);

    /**
     * @brief Exports a port scan diff to JSON format.
     * @param diff PortScanDiff to export.
     * @return JSON string representation of the diff.
     */
    std::string exportDiffToJson(const core::PortScanDiff& diff);

private:
    std::string portRangeToString(core::PortRange range);
    core::PortRange portRangeFromString(const std::string& str);
    std::string portsToString(const std::vector<uint16_t>& ports);
    std::vector<uint16_t> portsFromString(const std::string& str);

    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
