#pragma once

#include "core/types/SnmpTypes.hpp"
#include "infrastructure/database/Database.hpp"

#include <chrono>
#include <memory>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Repository for SNMP device configurations and monitoring results.
 *
 * Provides persistence operations for SNMP device configurations,
 * query results, OID value tracking, and historical data retrieval.
 */
class SnmpRepository {
public:
    /**
     * @brief Constructs an SnmpRepository with the given database.
     * @param db Shared pointer to the Database instance.
     */
    explicit SnmpRepository(std::shared_ptr<Database> db);

    /**
     * @brief Inserts a new SNMP device configuration.
     * @param config SnmpDeviceConfig to store.
     * @return ID of the inserted configuration.
     */
    int64_t insertDeviceConfig(const core::SnmpDeviceConfig& config);

    /**
     * @brief Updates an existing SNMP device configuration.
     * @param config SnmpDeviceConfig with updated values (id must be set).
     */
    void updateDeviceConfig(const core::SnmpDeviceConfig& config);

    /**
     * @brief Deletes an SNMP device configuration.
     * @param id ID of the configuration to delete.
     */
    void deleteDeviceConfig(int64_t id);

    /**
     * @brief Retrieves an SNMP device configuration by ID.
     * @param id ID of the configuration.
     * @return SnmpDeviceConfig if found, nullopt otherwise.
     */
    std::optional<core::SnmpDeviceConfig> getDeviceConfig(int64_t id);

    /**
     * @brief Retrieves an SNMP device configuration by host ID.
     * @param hostId ID of the associated host.
     * @return SnmpDeviceConfig if found, nullopt otherwise.
     */
    std::optional<core::SnmpDeviceConfig> getDeviceConfigByHostId(int64_t hostId);

    /**
     * @brief Retrieves all SNMP device configurations.
     * @return Vector of all SnmpDeviceConfig entities.
     */
    std::vector<core::SnmpDeviceConfig> getAllDeviceConfigs();

    /**
     * @brief Retrieves all enabled SNMP device configurations.
     * @return Vector of enabled configurations.
     */
    std::vector<core::SnmpDeviceConfig> getEnabledDeviceConfigs();

    /**
     * @brief Inserts an SNMP query result.
     * @param result SnmpResult to store.
     * @return ID of the inserted result.
     */
    int64_t insertResult(const core::SnmpResult& result);

    /**
     * @brief Retrieves SNMP results for a host.
     * @param hostId ID of the host.
     * @param limit Maximum number of results to return.
     * @return Vector of SNMP results, most recent first.
     */
    std::vector<core::SnmpResult> getResults(int64_t hostId, int limit = 100);

    /**
     * @brief Retrieves SNMP results since a specific time.
     * @param hostId ID of the host.
     * @param since Start time for the query.
     * @return Vector of SNMP results after the specified time.
     */
    std::vector<core::SnmpResult> getResultsSince(
        int64_t hostId, std::chrono::system_clock::time_point since);

    /**
     * @brief Calculates SNMP statistics for a host.
     * @param hostId ID of the host.
     * @param sampleCount Number of recent samples to include.
     * @return SnmpStatistics with success/failure counts and timing info.
     */
    core::SnmpStatistics getStatistics(int64_t hostId, int sampleCount = 100);

    /**
     * @brief Removes SNMP results older than the specified age.
     * @param maxAge Maximum age of records to keep.
     */
    void cleanupOldResults(std::chrono::hours maxAge);

    /**
     * @brief Inserts an OID value associated with a result.
     * @param resultId ID of the parent SNMP result.
     * @param varbind SnmpVarBind containing the OID and value.
     */
    void insertOidValue(int64_t resultId, const core::SnmpVarBind& varbind);

    /**
     * @brief Retrieves OID values for a specific result.
     * @param resultId ID of the SNMP result.
     * @return Vector of SnmpVarBind values.
     */
    std::vector<core::SnmpVarBind> getOidValues(int64_t resultId);

    /**
     * @brief Retrieves historical values for a specific OID.
     * @param hostId ID of the host.
     * @param oid OID string to query.
     * @param limit Maximum number of history entries.
     * @return Map of OID to timestamped value pairs.
     */
    std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>>
        getOidHistory(int64_t hostId, const std::string& oid, int limit = 100);

    /**
     * @brief Exports SNMP data for a host to JSON format.
     * @param hostId ID of the host.
     * @return JSON string containing SNMP data.
     */
    std::string exportToJson(int64_t hostId);

    /**
     * @brief Exports SNMP data for a host to CSV format.
     * @param hostId ID of the host.
     * @return CSV string containing SNMP data.
     */
    std::string exportToCsv(int64_t hostId);

    /**
     * @brief Creates the SNMP-related database tables.
     */
    void createTables();

private:
    std::string serializeCredentials(const core::SnmpCredentials& creds);
    core::SnmpCredentials deserializeCredentials(const std::string& json, core::SnmpVersion version);
    std::string serializeOids(const std::vector<std::string>& oids);
    std::vector<std::string> deserializeOids(const std::string& json);

    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
