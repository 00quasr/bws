/**
 * @file ScheduledScanViewModel.hpp
 * @brief ViewModel for scheduled port scan management.
 *
 * This file defines the ScheduledScanViewModel class which provides operations
 * for configuring and managing scheduled port scans and their results.
 */

#pragma once

#include "core/types/ScheduledPortScan.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/ScheduledScanRepository.hpp"
#include "infrastructure/network/ScheduledPortScanner.hpp"

#include <QObject>
#include <memory>
#include <optional>
#include <vector>

namespace netpulse::viewmodels {

/**
 * @brief ViewModel for managing scheduled port scans.
 *
 * Provides operations for creating, configuring, and running scheduled port
 * scans. Tracks scan differences over time and provides reporting capabilities.
 * Emits signals for UI updates when scan states change.
 */
class ScheduledScanViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a ScheduledScanViewModel.
     * @param db Shared pointer to the database connection.
     * @param scheduler Shared pointer to the scheduled port scanner service.
     * @param parent Optional parent QObject for Qt ownership.
     */
    explicit ScheduledScanViewModel(std::shared_ptr<infra::Database> db,
                                    std::shared_ptr<infra::ScheduledPortScanner> scheduler,
                                    QObject* parent = nullptr);

    /**
     * @brief Creates a new scheduled scan with default configuration.
     * @param name Display name for the schedule.
     * @param targetAddress IP address or hostname to scan.
     * @param portRange Range of ports to scan (default: Common).
     * @param intervalMinutes Scan interval in minutes (default: 60).
     * @return The ID of the newly created schedule.
     */
    int64_t addSchedule(const std::string& name, const std::string& targetAddress,
                        core::PortRange portRange = core::PortRange::Common,
                        int intervalMinutes = 60);

    /**
     * @brief Creates a new scheduled scan with full configuration.
     * @param config Complete scan configuration.
     * @return The ID of the newly created schedule.
     */
    int64_t addScheduleWithConfig(const core::ScheduledScanConfig& config);

    /**
     * @brief Updates an existing schedule's configuration.
     * @param config The configuration with updated fields.
     */
    void updateSchedule(const core::ScheduledScanConfig& config);

    /**
     * @brief Removes a scheduled scan.
     * @param scheduleId ID of the schedule to remove.
     */
    void removeSchedule(int64_t scheduleId);

    /**
     * @brief Enables or disables a scheduled scan.
     * @param scheduleId ID of the schedule to modify.
     * @param enabled True to enable, false to disable.
     */
    void setEnabled(int64_t scheduleId, bool enabled);

    /**
     * @brief Triggers an immediate execution of a scheduled scan.
     * @param scheduleId ID of the schedule to run.
     */
    void runNow(int64_t scheduleId);

    /**
     * @brief Gets a specific schedule by ID.
     * @param scheduleId ID of the schedule to retrieve.
     * @return The schedule configuration if found, std::nullopt otherwise.
     */
    std::optional<core::ScheduledScanConfig> getSchedule(int64_t scheduleId) const;

    /**
     * @brief Gets all scheduled scans.
     * @return Vector of all schedule configurations.
     */
    std::vector<core::ScheduledScanConfig> getAllSchedules() const;

    /**
     * @brief Gets all enabled scheduled scans.
     * @return Vector of enabled schedule configurations.
     */
    std::vector<core::ScheduledScanConfig> getEnabledSchedules() const;

    /**
     * @brief Gets port scan differences for a schedule.
     * @param scheduleId ID of the schedule to query.
     * @param limit Maximum number of diffs to return (default: 100).
     * @return Vector of port scan diffs, ordered by timestamp descending.
     */
    std::vector<core::PortScanDiff> getDiffs(int64_t scheduleId, int limit = 100) const;

    /**
     * @brief Gets port scan differences for an address.
     * @param address Target address to query.
     * @param limit Maximum number of diffs to return (default: 100).
     * @return Vector of port scan diffs for the address.
     */
    std::vector<core::PortScanDiff> getDiffsByAddress(const std::string& address,
                                                       int limit = 100) const;

    /**
     * @brief Exports a single diff as a formatted report.
     * @param diff The port scan diff to export.
     * @return Formatted report string.
     */
    std::string exportDiffReport(const core::PortScanDiff& diff) const;

    /**
     * @brief Exports all diffs for a schedule as a formatted report.
     * @param scheduleId ID of the schedule to export.
     * @return Formatted report string with all diffs.
     */
    std::string exportAllDiffsReport(int64_t scheduleId) const;

    /**
     * @brief Starts the scheduler to run enabled scans.
     */
    void startScheduler();

    /**
     * @brief Stops the scheduler.
     */
    void stopScheduler();

    /**
     * @brief Checks if the scheduler is currently running.
     * @return True if the scheduler is active.
     */
    bool isSchedulerRunning() const;

signals:
    /**
     * @brief Emitted when a new schedule is created.
     * @param scheduleId ID of the newly created schedule.
     */
    void scheduleAdded(int64_t scheduleId);

    /**
     * @brief Emitted when a schedule is updated.
     * @param scheduleId ID of the updated schedule.
     */
    void scheduleUpdated(int64_t scheduleId);

    /**
     * @brief Emitted when a schedule is removed.
     * @param scheduleId ID of the removed schedule.
     */
    void scheduleRemoved(int64_t scheduleId);

    /**
     * @brief Emitted when a scheduled scan completes.
     * @param scheduleId ID of the completed schedule.
     * @param portsScanned Total number of ports scanned.
     * @param openPorts Number of open ports found.
     */
    void scanCompleted(int64_t scheduleId, int portsScanned, int openPorts);

    /**
     * @brief Emitted when port changes are detected.
     * @param scheduleId ID of the schedule with changes.
     * @param changesCount Number of port state changes detected.
     */
    void diffDetected(int64_t scheduleId, int changesCount);

    /**
     * @brief Emitted when the scheduler starts.
     */
    void schedulerStarted();

    /**
     * @brief Emitted when the scheduler stops.
     */
    void schedulerStopped();

private:
    void onScanComplete(int64_t scheduleId, const std::vector<core::PortScanResult>& results);
    void onDiffDetected(int64_t scheduleId, const core::PortScanDiff& diff);
    void loadSchedulesFromDatabase();

    std::shared_ptr<infra::Database> db_;
    std::shared_ptr<infra::ScheduledPortScanner> scheduler_;
    std::unique_ptr<infra::ScheduledScanRepository> repo_;
};

} // namespace netpulse::viewmodels
