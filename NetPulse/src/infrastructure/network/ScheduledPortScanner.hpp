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

/**
 * @brief Manages scheduled port scans with diff tracking capabilities.
 *
 * Provides automated periodic port scanning with the ability to detect
 * changes between scans. Supports multiple schedules with independent
 * intervals and configurations. Implements core::IScheduledPortScanner.
 */
class ScheduledPortScanner : public core::IScheduledPortScanner {
public:
    /**
     * @brief Constructs a ScheduledPortScanner.
     * @param context Reference to the AsioContext for timer management.
     * @param portScanner Reference to the port scanner to use for scans.
     */
    ScheduledPortScanner(AsioContext& context, core::IPortScanner& portScanner);

    /**
     * @brief Destructor. Stops the scheduler and releases resources.
     */
    ~ScheduledPortScanner() override;

    /**
     * @brief Adds a new scan schedule.
     * @param config The schedule configuration to add.
     */
    void addSchedule(const core::ScheduledScanConfig& config) override;

    /**
     * @brief Updates an existing scan schedule.
     * @param config The updated schedule configuration (id must match existing).
     */
    void updateSchedule(const core::ScheduledScanConfig& config) override;

    /**
     * @brief Removes a scan schedule.
     * @param scheduleId ID of the schedule to remove.
     */
    void removeSchedule(int64_t scheduleId) override;

    /**
     * @brief Enables or disables a schedule.
     * @param scheduleId ID of the schedule to modify.
     * @param enabled True to enable, false to disable.
     */
    void enableSchedule(int64_t scheduleId, bool enabled) override;

    /**
     * @brief Retrieves all registered schedules.
     * @return Vector of all schedule configurations.
     */
    std::vector<core::ScheduledScanConfig> getSchedules() const override;

    /**
     * @brief Retrieves a specific schedule by ID.
     * @param scheduleId ID of the schedule to retrieve.
     * @return The schedule if found, nullopt otherwise.
     */
    std::optional<core::ScheduledScanConfig> getSchedule(int64_t scheduleId) const override;

    /**
     * @brief Executes a scheduled scan immediately.
     * @param scheduleId ID of the schedule to run.
     */
    void runNow(int64_t scheduleId) override;

    /**
     * @brief Starts the scheduler and begins processing schedules.
     */
    void start() override;

    /**
     * @brief Stops the scheduler and cancels all pending scans.
     */
    void stop() override;

    /**
     * @brief Checks if the scheduler is currently running.
     * @return True if running, false otherwise.
     */
    bool isRunning() const override;

    /**
     * @brief Sets the callback for scan completion events.
     * @param callback Function called when a scan completes.
     */
    void setScanCompleteCallback(ScanCompleteCallback callback) override;

    /**
     * @brief Sets the callback for scan diff events.
     * @param callback Function called when changes are detected.
     */
    void setDiffCallback(DiffCallback callback) override;

    /**
     * @brief Computes the difference between two scan result sets.
     * @param targetAddress The target address for the diff.
     * @param previous Previous scan results.
     * @param current Current scan results.
     * @return PortScanDiff describing the changes.
     */
    core::PortScanDiff computeDiff(const std::string& targetAddress,
                                    const std::vector<core::PortScanResult>& previous,
                                    const std::vector<core::PortScanResult>& current) override;

    /**
     * @brief Stores the last scan results for a schedule.
     * @param scheduleId ID of the schedule.
     * @param results The scan results to store.
     */
    void setLastScanResults(int64_t scheduleId, std::vector<core::PortScanResult> results);

    /**
     * @brief Retrieves the last scan results for a schedule.
     * @param scheduleId ID of the schedule.
     * @return Vector of the last scan results.
     */
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
