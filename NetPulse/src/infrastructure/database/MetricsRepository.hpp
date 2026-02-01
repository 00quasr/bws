#pragma once

#include "core/types/Alert.hpp"
#include "core/types/PingResult.hpp"
#include "core/types/PortScanResult.hpp"
#include "infrastructure/database/Database.hpp"

#include <chrono>
#include <memory>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Repository for metrics data persistence (ping results, alerts, port scans).
 *
 * Provides storage and retrieval operations for monitoring metrics including
 * ping results, alerts, and port scan data. Supports data export and cleanup.
 */
class MetricsRepository {
public:
    /**
     * @brief Constructs a MetricsRepository with the given database.
     * @param db Shared pointer to the Database instance.
     */
    explicit MetricsRepository(std::shared_ptr<Database> db);

    /**
     * @brief Inserts a ping result into the database.
     * @param result PingResult to store.
     * @return ID of the inserted record.
     */
    int64_t insertPingResult(const core::PingResult& result);

    /**
     * @brief Retrieves ping results for a host.
     * @param hostId ID of the host.
     * @param limit Maximum number of results to return.
     * @return Vector of ping results, most recent first.
     */
    std::vector<core::PingResult> getPingResults(int64_t hostId, int limit = 100);

    /**
     * @brief Retrieves ping results since a specific time.
     * @param hostId ID of the host.
     * @param since Start time for the query.
     * @return Vector of ping results after the specified time.
     */
    std::vector<core::PingResult> getPingResultsSince(
        int64_t hostId, std::chrono::system_clock::time_point since);

    /**
     * @brief Calculates ping statistics for a host.
     * @param hostId ID of the host.
     * @param sampleCount Number of recent samples to include.
     * @return PingStatistics with min/max/avg latency and loss rate.
     */
    core::PingStatistics getStatistics(int64_t hostId, int sampleCount = 100);

    /**
     * @brief Removes ping results older than the specified age.
     * @param maxAge Maximum age of records to keep.
     */
    void cleanupOldPingResults(std::chrono::hours maxAge);

    /**
     * @brief Inserts an alert into the database.
     * @param alert Alert to store.
     * @return ID of the inserted alert.
     */
    int64_t insertAlert(const core::Alert& alert);

    /**
     * @brief Retrieves recent alerts.
     * @param limit Maximum number of alerts to return.
     * @return Vector of alerts, most recent first.
     */
    std::vector<core::Alert> getAlerts(int limit = 100);

    /**
     * @brief Retrieves alerts matching filter criteria.
     * @param filter Filter criteria for alerts.
     * @param limit Maximum number of alerts to return.
     * @return Vector of matching alerts.
     */
    std::vector<core::Alert> getAlertsFiltered(const core::AlertFilter& filter, int limit = 100);

    /**
     * @brief Retrieves all unacknowledged alerts.
     * @return Vector of unacknowledged alerts.
     */
    std::vector<core::Alert> getUnacknowledgedAlerts();

    /**
     * @brief Marks an alert as acknowledged.
     * @param id ID of the alert to acknowledge.
     */
    void acknowledgeAlert(int64_t id);

    /**
     * @brief Acknowledges all unacknowledged alerts.
     */
    void acknowledgeAll();

    /**
     * @brief Removes alerts older than the specified age.
     * @param maxAge Maximum age of alerts to keep.
     */
    void cleanupOldAlerts(std::chrono::hours maxAge);

    /**
     * @brief Inserts a single port scan result.
     * @param result PortScanResult to store.
     * @return ID of the inserted record.
     */
    int64_t insertPortScanResult(const core::PortScanResult& result);

    /**
     * @brief Inserts multiple port scan results in a batch.
     * @param results Vector of PortScanResult to store.
     */
    void insertPortScanResults(const std::vector<core::PortScanResult>& results);

    /**
     * @brief Retrieves port scan results for an address.
     * @param address Target address to query.
     * @param limit Maximum number of results to return.
     * @return Vector of port scan results.
     */
    std::vector<core::PortScanResult> getPortScanResults(const std::string& address,
                                                          int limit = 1000);

    /**
     * @brief Removes port scan results older than the specified age.
     * @param maxAge Maximum age of records to keep.
     */
    void cleanupOldPortScans(std::chrono::hours maxAge);

    /**
     * @brief Exports host metrics data to JSON format.
     * @param hostId ID of the host.
     * @return JSON string containing all metrics for the host.
     */
    std::string exportToJson(int64_t hostId);

    /**
     * @brief Exports host metrics data to CSV format.
     * @param hostId ID of the host.
     * @return CSV string containing all metrics for the host.
     */
    std::string exportToCsv(int64_t hostId);

private:
    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
