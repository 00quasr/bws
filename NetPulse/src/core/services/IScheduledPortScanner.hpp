/**
 * @file IScheduledPortScanner.hpp
 * @brief Interface for the scheduled port scanning service.
 *
 * This file defines the abstract interface for managing scheduled port scans
 * and detecting changes between scan runs.
 */

#pragma once

#include "core/types/ScheduledPortScan.hpp"

#include <functional>
#include <vector>

namespace netpulse::core {

/**
 * @brief Interface for scheduled port scanning service.
 *
 * Provides methods for managing periodic port scan schedules, running
 * scans on demand, and comparing results to detect changes.
 */
class IScheduledPortScanner {
public:
    /**
     * @brief Callback function type for scan completion.
     * @param scheduleId ID of the schedule that completed.
     * @param results Vector of port scan results.
     */
    using ScanCompleteCallback = std::function<void(int64_t scheduleId, const std::vector<PortScanResult>&)>;

    /**
     * @brief Callback function type for port state changes.
     * @param scheduleId ID of the schedule that detected changes.
     * @param diff The difference between scans.
     */
    using DiffCallback = std::function<void(int64_t scheduleId, const PortScanDiff&)>;

    virtual ~IScheduledPortScanner() = default;

    /**
     * @brief Adds a new scheduled scan configuration.
     * @param config The schedule configuration to add.
     */
    virtual void addSchedule(const ScheduledScanConfig& config) = 0;

    /**
     * @brief Updates an existing scheduled scan configuration.
     * @param config The updated configuration (matched by ID).
     */
    virtual void updateSchedule(const ScheduledScanConfig& config) = 0;

    /**
     * @brief Removes a scheduled scan.
     * @param scheduleId ID of the schedule to remove.
     */
    virtual void removeSchedule(int64_t scheduleId) = 0;

    /**
     * @brief Enables or disables a scheduled scan.
     * @param scheduleId ID of the schedule.
     * @param enabled True to enable, false to disable.
     */
    virtual void enableSchedule(int64_t scheduleId, bool enabled) = 0;

    /**
     * @brief Gets all scheduled scan configurations.
     * @return Vector of all schedule configurations.
     */
    virtual std::vector<ScheduledScanConfig> getSchedules() const = 0;

    /**
     * @brief Gets a specific scheduled scan configuration.
     * @param scheduleId ID of the schedule to retrieve.
     * @return The schedule configuration if found, std::nullopt otherwise.
     */
    virtual std::optional<ScheduledScanConfig> getSchedule(int64_t scheduleId) const = 0;

    /**
     * @brief Runs a scheduled scan immediately.
     * @param scheduleId ID of the schedule to run.
     */
    virtual void runNow(int64_t scheduleId) = 0;

    /**
     * @brief Starts the scheduler.
     */
    virtual void start() = 0;

    /**
     * @brief Stops the scheduler.
     */
    virtual void stop() = 0;

    /**
     * @brief Checks if the scheduler is running.
     * @return True if the scheduler is active.
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Sets the callback for scan completion events.
     * @param callback Function to call when a scheduled scan completes.
     */
    virtual void setScanCompleteCallback(ScanCompleteCallback callback) = 0;

    /**
     * @brief Sets the callback for port state change events.
     * @param callback Function to call when port changes are detected.
     */
    virtual void setDiffCallback(DiffCallback callback) = 0;

    /**
     * @brief Computes the difference between two scan results.
     * @param targetAddress The target address that was scanned.
     * @param previous Results from the previous scan.
     * @param current Results from the current scan.
     * @return The differences between the two scans.
     */
    virtual PortScanDiff computeDiff(const std::string& targetAddress,
                                      const std::vector<PortScanResult>& previous,
                                      const std::vector<PortScanResult>& current) = 0;
};

} // namespace netpulse::core
