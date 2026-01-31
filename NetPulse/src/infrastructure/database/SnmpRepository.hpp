#pragma once

#include "core/types/SnmpTypes.hpp"
#include "infrastructure/database/Database.hpp"

#include <chrono>
#include <memory>
#include <vector>

namespace netpulse::infra {

class SnmpRepository {
public:
    explicit SnmpRepository(std::shared_ptr<Database> db);

    // SNMP device configuration
    int64_t insertDeviceConfig(const core::SnmpDeviceConfig& config);
    void updateDeviceConfig(const core::SnmpDeviceConfig& config);
    void deleteDeviceConfig(int64_t id);
    std::optional<core::SnmpDeviceConfig> getDeviceConfig(int64_t id);
    std::optional<core::SnmpDeviceConfig> getDeviceConfigByHostId(int64_t hostId);
    std::vector<core::SnmpDeviceConfig> getAllDeviceConfigs();
    std::vector<core::SnmpDeviceConfig> getEnabledDeviceConfigs();

    // SNMP results
    int64_t insertResult(const core::SnmpResult& result);
    std::vector<core::SnmpResult> getResults(int64_t hostId, int limit = 100);
    std::vector<core::SnmpResult> getResultsSince(
        int64_t hostId, std::chrono::system_clock::time_point since);
    core::SnmpStatistics getStatistics(int64_t hostId, int sampleCount = 100);
    void cleanupOldResults(std::chrono::hours maxAge);

    // OID values tracking
    void insertOidValue(int64_t resultId, const core::SnmpVarBind& varbind);
    std::vector<core::SnmpVarBind> getOidValues(int64_t resultId);
    std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>>
        getOidHistory(int64_t hostId, const std::string& oid, int limit = 100);

    // Export
    std::string exportToJson(int64_t hostId);
    std::string exportToCsv(int64_t hostId);

    // Schema creation/migration
    void createTables();

private:
    std::string serializeCredentials(const core::SnmpCredentials& creds);
    core::SnmpCredentials deserializeCredentials(const std::string& json, core::SnmpVersion version);
    std::string serializeOids(const std::vector<std::string>& oids);
    std::vector<std::string> deserializeOids(const std::string& json);

    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
