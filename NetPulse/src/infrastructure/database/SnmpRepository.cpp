#include "infrastructure/database/SnmpRepository.hpp"

#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>

namespace netpulse::infra {

namespace {

std::string timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&time, &tm);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return buffer;
}

std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) {
    std::tm tm{};
    strptime(str.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}

} // namespace

SnmpRepository::SnmpRepository(std::shared_ptr<Database> db) : db_(std::move(db)) {
    createTables();
}

void SnmpRepository::createTables() {
    // SNMP device configurations table
    db_->execute(R"(
        CREATE TABLE IF NOT EXISTS snmp_device_configs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            host_id INTEGER NOT NULL UNIQUE,
            version INTEGER NOT NULL DEFAULT 2,
            credentials TEXT NOT NULL,
            port INTEGER NOT NULL DEFAULT 161,
            timeout_ms INTEGER NOT NULL DEFAULT 5000,
            retries INTEGER NOT NULL DEFAULT 1,
            poll_interval_seconds INTEGER NOT NULL DEFAULT 60,
            oids TEXT NOT NULL,
            enabled INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL,
            last_polled TEXT,
            FOREIGN KEY (host_id) REFERENCES hosts(id) ON DELETE CASCADE
        )
    )");

    // SNMP results table
    db_->execute(R"(
        CREATE TABLE IF NOT EXISTS snmp_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            host_id INTEGER NOT NULL,
            timestamp TEXT NOT NULL,
            version INTEGER NOT NULL,
            response_time_us INTEGER NOT NULL,
            success INTEGER NOT NULL,
            error_message TEXT,
            error_status INTEGER DEFAULT 0,
            error_index INTEGER DEFAULT 0,
            FOREIGN KEY (host_id) REFERENCES hosts(id) ON DELETE CASCADE
        )
    )");

    // SNMP OID values table (stores individual variable bindings)
    db_->execute(R"(
        CREATE TABLE IF NOT EXISTS snmp_oid_values (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            result_id INTEGER NOT NULL,
            oid TEXT NOT NULL,
            data_type INTEGER NOT NULL,
            value TEXT,
            int_value INTEGER,
            counter_value INTEGER,
            FOREIGN KEY (result_id) REFERENCES snmp_results(id) ON DELETE CASCADE
        )
    )");

    // Indexes for performance
    db_->execute("CREATE INDEX IF NOT EXISTS idx_snmp_results_host_time ON snmp_results(host_id, timestamp)");
    db_->execute("CREATE INDEX IF NOT EXISTS idx_snmp_oid_values_result ON snmp_oid_values(result_id)");
    db_->execute("CREATE INDEX IF NOT EXISTS idx_snmp_device_configs_host ON snmp_device_configs(host_id)");

    spdlog::debug("SNMP tables created/verified");
}

int64_t SnmpRepository::insertDeviceConfig(const core::SnmpDeviceConfig& config) {
    auto stmt = db_->prepare(R"(
        INSERT INTO snmp_device_configs
            (host_id, version, credentials, port, timeout_ms, retries,
             poll_interval_seconds, oids, enabled, created_at, last_polled)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    stmt.bind(1, config.hostId);
    stmt.bind(2, static_cast<int>(config.version));
    stmt.bind(3, serializeCredentials(config.credentials));
    stmt.bind(4, static_cast<int>(config.port));
    stmt.bind(5, config.timeoutMs);
    stmt.bind(6, config.retries);
    stmt.bind(7, config.pollIntervalSeconds);
    stmt.bind(8, serializeOids(config.oids));
    stmt.bind(9, config.enabled ? 1 : 0);
    stmt.bind(10, timePointToString(config.createdAt));
    if (config.lastPolled) {
        stmt.bind(11, timePointToString(*config.lastPolled));
    } else {
        stmt.bindNull(11);
    }

    stmt.step();
    return db_->lastInsertRowId();
}

void SnmpRepository::updateDeviceConfig(const core::SnmpDeviceConfig& config) {
    auto stmt = db_->prepare(R"(
        UPDATE snmp_device_configs SET
            version = ?, credentials = ?, port = ?, timeout_ms = ?, retries = ?,
            poll_interval_seconds = ?, oids = ?, enabled = ?, last_polled = ?
        WHERE id = ?
    )");

    stmt.bind(1, static_cast<int>(config.version));
    stmt.bind(2, serializeCredentials(config.credentials));
    stmt.bind(3, static_cast<int>(config.port));
    stmt.bind(4, config.timeoutMs);
    stmt.bind(5, config.retries);
    stmt.bind(6, config.pollIntervalSeconds);
    stmt.bind(7, serializeOids(config.oids));
    stmt.bind(8, config.enabled ? 1 : 0);
    if (config.lastPolled) {
        stmt.bind(9, timePointToString(*config.lastPolled));
    } else {
        stmt.bindNull(9);
    }
    stmt.bind(10, config.id);

    stmt.step();
}

void SnmpRepository::deleteDeviceConfig(int64_t id) {
    auto stmt = db_->prepare("DELETE FROM snmp_device_configs WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
}

std::optional<core::SnmpDeviceConfig> SnmpRepository::getDeviceConfig(int64_t id) {
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, version, credentials, port, timeout_ms, retries,
               poll_interval_seconds, oids, enabled, created_at, last_polled
        FROM snmp_device_configs WHERE id = ?
    )");

    stmt.bind(1, id);

    if (stmt.step()) {
        core::SnmpDeviceConfig config;
        config.id = stmt.columnInt64(0);
        config.hostId = stmt.columnInt64(1);
        config.version = static_cast<core::SnmpVersion>(stmt.columnInt(2));
        config.credentials = deserializeCredentials(stmt.columnText(3), config.version);
        config.port = static_cast<uint16_t>(stmt.columnInt(4));
        config.timeoutMs = stmt.columnInt(5);
        config.retries = stmt.columnInt(6);
        config.pollIntervalSeconds = stmt.columnInt(7);
        config.oids = deserializeOids(stmt.columnText(8));
        config.enabled = stmt.columnInt(9) != 0;
        config.createdAt = stringToTimePoint(stmt.columnText(10));
        if (!stmt.columnIsNull(11)) {
            config.lastPolled = stringToTimePoint(stmt.columnText(11));
        }
        return config;
    }

    return std::nullopt;
}

std::optional<core::SnmpDeviceConfig> SnmpRepository::getDeviceConfigByHostId(int64_t hostId) {
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, version, credentials, port, timeout_ms, retries,
               poll_interval_seconds, oids, enabled, created_at, last_polled
        FROM snmp_device_configs WHERE host_id = ?
    )");

    stmt.bind(1, hostId);

    if (stmt.step()) {
        core::SnmpDeviceConfig config;
        config.id = stmt.columnInt64(0);
        config.hostId = stmt.columnInt64(1);
        config.version = static_cast<core::SnmpVersion>(stmt.columnInt(2));
        config.credentials = deserializeCredentials(stmt.columnText(3), config.version);
        config.port = static_cast<uint16_t>(stmt.columnInt(4));
        config.timeoutMs = stmt.columnInt(5);
        config.retries = stmt.columnInt(6);
        config.pollIntervalSeconds = stmt.columnInt(7);
        config.oids = deserializeOids(stmt.columnText(8));
        config.enabled = stmt.columnInt(9) != 0;
        config.createdAt = stringToTimePoint(stmt.columnText(10));
        if (!stmt.columnIsNull(11)) {
            config.lastPolled = stringToTimePoint(stmt.columnText(11));
        }
        return config;
    }

    return std::nullopt;
}

std::vector<core::SnmpDeviceConfig> SnmpRepository::getAllDeviceConfigs() {
    std::vector<core::SnmpDeviceConfig> configs;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, version, credentials, port, timeout_ms, retries,
               poll_interval_seconds, oids, enabled, created_at, last_polled
        FROM snmp_device_configs ORDER BY id
    )");

    while (stmt.step()) {
        core::SnmpDeviceConfig config;
        config.id = stmt.columnInt64(0);
        config.hostId = stmt.columnInt64(1);
        config.version = static_cast<core::SnmpVersion>(stmt.columnInt(2));
        config.credentials = deserializeCredentials(stmt.columnText(3), config.version);
        config.port = static_cast<uint16_t>(stmt.columnInt(4));
        config.timeoutMs = stmt.columnInt(5);
        config.retries = stmt.columnInt(6);
        config.pollIntervalSeconds = stmt.columnInt(7);
        config.oids = deserializeOids(stmt.columnText(8));
        config.enabled = stmt.columnInt(9) != 0;
        config.createdAt = stringToTimePoint(stmt.columnText(10));
        if (!stmt.columnIsNull(11)) {
            config.lastPolled = stringToTimePoint(stmt.columnText(11));
        }
        configs.push_back(config);
    }

    return configs;
}

std::vector<core::SnmpDeviceConfig> SnmpRepository::getEnabledDeviceConfigs() {
    std::vector<core::SnmpDeviceConfig> configs;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, version, credentials, port, timeout_ms, retries,
               poll_interval_seconds, oids, enabled, created_at, last_polled
        FROM snmp_device_configs WHERE enabled = 1 ORDER BY id
    )");

    while (stmt.step()) {
        core::SnmpDeviceConfig config;
        config.id = stmt.columnInt64(0);
        config.hostId = stmt.columnInt64(1);
        config.version = static_cast<core::SnmpVersion>(stmt.columnInt(2));
        config.credentials = deserializeCredentials(stmt.columnText(3), config.version);
        config.port = static_cast<uint16_t>(stmt.columnInt(4));
        config.timeoutMs = stmt.columnInt(5);
        config.retries = stmt.columnInt(6);
        config.pollIntervalSeconds = stmt.columnInt(7);
        config.oids = deserializeOids(stmt.columnText(8));
        config.enabled = stmt.columnInt(9) != 0;
        config.createdAt = stringToTimePoint(stmt.columnText(10));
        if (!stmt.columnIsNull(11)) {
            config.lastPolled = stringToTimePoint(stmt.columnText(11));
        }
        configs.push_back(config);
    }

    return configs;
}

int64_t SnmpRepository::insertResult(const core::SnmpResult& result) {
    auto stmt = db_->prepare(R"(
        INSERT INTO snmp_results
            (host_id, timestamp, version, response_time_us, success, error_message, error_status, error_index)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");

    stmt.bind(1, result.hostId);
    stmt.bind(2, timePointToString(result.timestamp));
    stmt.bind(3, static_cast<int>(result.version));
    stmt.bind(4, result.responseTime.count());
    stmt.bind(5, result.success ? 1 : 0);
    stmt.bind(6, result.errorMessage);
    stmt.bind(7, result.errorStatus);
    stmt.bind(8, result.errorIndex);

    stmt.step();
    int64_t resultId = db_->lastInsertRowId();

    // Insert varbinds
    for (const auto& vb : result.varbinds) {
        insertOidValue(resultId, vb);
    }

    return resultId;
}

std::vector<core::SnmpResult> SnmpRepository::getResults(int64_t hostId, int limit) {
    std::vector<core::SnmpResult> results;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, timestamp, version, response_time_us, success, error_message, error_status, error_index
        FROM snmp_results WHERE host_id = ?
        ORDER BY timestamp DESC LIMIT ?
    )");

    stmt.bind(1, hostId);
    stmt.bind(2, limit);

    while (stmt.step()) {
        core::SnmpResult result;
        result.id = stmt.columnInt64(0);
        result.hostId = stmt.columnInt64(1);
        result.timestamp = stringToTimePoint(stmt.columnText(2));
        result.version = static_cast<core::SnmpVersion>(stmt.columnInt(3));
        result.responseTime = std::chrono::microseconds(stmt.columnInt64(4));
        result.success = stmt.columnInt(5) != 0;
        result.errorMessage = stmt.columnText(6);
        result.errorStatus = stmt.columnInt(7);
        result.errorIndex = stmt.columnInt(8);

        // Load varbinds
        result.varbinds = getOidValues(result.id);

        results.push_back(result);
    }

    return results;
}

std::vector<core::SnmpResult> SnmpRepository::getResultsSince(
    int64_t hostId, std::chrono::system_clock::time_point since) {
    std::vector<core::SnmpResult> results;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, timestamp, version, response_time_us, success, error_message, error_status, error_index
        FROM snmp_results WHERE host_id = ? AND timestamp >= ?
        ORDER BY timestamp ASC
    )");

    stmt.bind(1, hostId);
    stmt.bind(2, timePointToString(since));

    while (stmt.step()) {
        core::SnmpResult result;
        result.id = stmt.columnInt64(0);
        result.hostId = stmt.columnInt64(1);
        result.timestamp = stringToTimePoint(stmt.columnText(2));
        result.version = static_cast<core::SnmpVersion>(stmt.columnInt(3));
        result.responseTime = std::chrono::microseconds(stmt.columnInt64(4));
        result.success = stmt.columnInt(5) != 0;
        result.errorMessage = stmt.columnText(6);
        result.errorStatus = stmt.columnInt(7);
        result.errorIndex = stmt.columnInt(8);

        // Load varbinds
        result.varbinds = getOidValues(result.id);

        results.push_back(result);
    }

    return results;
}

core::SnmpStatistics SnmpRepository::getStatistics(int64_t hostId, int sampleCount) {
    core::SnmpStatistics stats;
    stats.hostId = hostId;

    auto stmt = db_->prepare(R"(
        SELECT
            COUNT(*) as total,
            SUM(CASE WHEN success = 1 THEN 1 ELSE 0 END) as successful,
            MIN(CASE WHEN success = 1 THEN response_time_us END) as min_resp,
            MAX(CASE WHEN success = 1 THEN response_time_us END) as max_resp,
            AVG(CASE WHEN success = 1 THEN response_time_us END) as avg_resp
        FROM (
            SELECT * FROM snmp_results WHERE host_id = ?
            ORDER BY timestamp DESC LIMIT ?
        )
    )");

    stmt.bind(1, hostId);
    stmt.bind(2, sampleCount);

    if (stmt.step()) {
        stats.totalPolls = stmt.columnInt(0);
        stats.successfulPolls = stmt.columnInt(1);

        if (!stmt.columnIsNull(2)) {
            stats.minResponseTime = std::chrono::microseconds(stmt.columnInt64(2));
        }
        if (!stmt.columnIsNull(3)) {
            stats.maxResponseTime = std::chrono::microseconds(stmt.columnInt64(3));
        }
        if (!stmt.columnIsNull(4)) {
            stats.avgResponseTime = std::chrono::microseconds(static_cast<int64_t>(stmt.columnDouble(4)));
        }

        stats.successRate = stats.calculateSuccessRate();
    }

    // Get last values for each OID
    auto lastValuesStmt = db_->prepare(R"(
        SELECT ov.oid, ov.value
        FROM snmp_oid_values ov
        INNER JOIN snmp_results sr ON ov.result_id = sr.id
        WHERE sr.host_id = ? AND sr.success = 1
        ORDER BY sr.timestamp DESC
        LIMIT 100
    )");

    lastValuesStmt.bind(1, hostId);

    std::set<std::string> seenOids;
    while (lastValuesStmt.step()) {
        std::string oid = lastValuesStmt.columnText(0);
        if (seenOids.find(oid) == seenOids.end()) {
            stats.lastValues[oid] = lastValuesStmt.columnText(1);
            seenOids.insert(oid);
        }
    }

    return stats;
}

void SnmpRepository::cleanupOldResults(std::chrono::hours maxAge) {
    auto cutoff = std::chrono::system_clock::now() - maxAge;

    // First delete OID values for old results
    auto deleteValuesStmt = db_->prepare(R"(
        DELETE FROM snmp_oid_values WHERE result_id IN (
            SELECT id FROM snmp_results WHERE timestamp < ?
        )
    )");
    deleteValuesStmt.bind(1, timePointToString(cutoff));
    deleteValuesStmt.step();

    // Then delete results
    auto stmt = db_->prepare("DELETE FROM snmp_results WHERE timestamp < ?");
    stmt.bind(1, timePointToString(cutoff));
    stmt.step();

    spdlog::info("Cleaned up SNMP results older than {} hours", maxAge.count());
}

void SnmpRepository::insertOidValue(int64_t resultId, const core::SnmpVarBind& varbind) {
    auto stmt = db_->prepare(R"(
        INSERT INTO snmp_oid_values (result_id, oid, data_type, value, int_value, counter_value)
        VALUES (?, ?, ?, ?, ?, ?)
    )");

    stmt.bind(1, resultId);
    stmt.bind(2, varbind.oid);
    stmt.bind(3, static_cast<int>(varbind.type));
    stmt.bind(4, varbind.value);
    if (varbind.intValue) {
        stmt.bind(5, *varbind.intValue);
    } else {
        stmt.bindNull(5);
    }
    if (varbind.counterValue) {
        stmt.bind(6, static_cast<int64_t>(*varbind.counterValue));
    } else {
        stmt.bindNull(6);
    }

    stmt.step();
}

std::vector<core::SnmpVarBind> SnmpRepository::getOidValues(int64_t resultId) {
    std::vector<core::SnmpVarBind> varbinds;
    auto stmt = db_->prepare(R"(
        SELECT oid, data_type, value, int_value, counter_value
        FROM snmp_oid_values WHERE result_id = ?
    )");

    stmt.bind(1, resultId);

    while (stmt.step()) {
        core::SnmpVarBind vb;
        vb.oid = stmt.columnText(0);
        vb.type = static_cast<core::SnmpDataType>(stmt.columnInt(1));
        vb.value = stmt.columnText(2);
        if (!stmt.columnIsNull(3)) {
            vb.intValue = stmt.columnInt64(3);
        }
        if (!stmt.columnIsNull(4)) {
            vb.counterValue = static_cast<uint64_t>(stmt.columnInt64(4));
        }
        varbinds.push_back(vb);
    }

    return varbinds;
}

std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>>
SnmpRepository::getOidHistory(int64_t hostId, const std::string& oid, int limit) {
    std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>> history;

    auto stmt = db_->prepare(R"(
        SELECT sr.timestamp, ov.oid, ov.value
        FROM snmp_oid_values ov
        INNER JOIN snmp_results sr ON ov.result_id = sr.id
        WHERE sr.host_id = ? AND ov.oid LIKE ?
        ORDER BY sr.timestamp DESC
        LIMIT ?
    )");

    stmt.bind(1, hostId);
    stmt.bind(2, oid + "%");
    stmt.bind(3, limit);

    while (stmt.step()) {
        auto timestamp = stringToTimePoint(stmt.columnText(0));
        std::string foundOid = stmt.columnText(1);
        std::string value = stmt.columnText(2);

        history[foundOid].emplace_back(timestamp, value);
    }

    return history;
}

std::string SnmpRepository::exportToJson(int64_t hostId) {
    nlohmann::json j;

    auto results = getResults(hostId, 10000);
    j["host_id"] = hostId;
    j["export_time"] = timePointToString(std::chrono::system_clock::now());
    j["snmp_results"] = nlohmann::json::array();

    for (const auto& r : results) {
        nlohmann::json entry;
        entry["timestamp"] = timePointToString(r.timestamp);
        entry["version"] = core::snmpVersionToString(r.version);
        entry["response_time_ms"] = r.responseTimeMs();
        entry["success"] = r.success;
        if (!r.errorMessage.empty()) {
            entry["error_message"] = r.errorMessage;
        }

        entry["varbinds"] = nlohmann::json::array();
        for (const auto& vb : r.varbinds) {
            nlohmann::json vbEntry;
            vbEntry["oid"] = vb.oid;
            vbEntry["type"] = core::snmpDataTypeToString(vb.type);
            vbEntry["value"] = vb.value;
            entry["varbinds"].push_back(vbEntry);
        }

        j["snmp_results"].push_back(entry);
    }

    auto stats = getStatistics(hostId);
    j["statistics"]["total_polls"] = stats.totalPolls;
    j["statistics"]["successful_polls"] = stats.successfulPolls;
    j["statistics"]["success_rate"] = stats.successRate;
    j["statistics"]["min_response_time_ms"] = stats.minResponseTime.count() / 1000.0;
    j["statistics"]["max_response_time_ms"] = stats.maxResponseTime.count() / 1000.0;
    j["statistics"]["avg_response_time_ms"] = stats.avgResponseTime.count() / 1000.0;

    return j.dump(2);
}

std::string SnmpRepository::exportToCsv(int64_t hostId) {
    std::ostringstream oss;
    oss << "timestamp,version,response_time_ms,success,oid,type,value\n";

    auto results = getResults(hostId, 10000);
    for (const auto& r : results) {
        for (const auto& vb : r.varbinds) {
            oss << timePointToString(r.timestamp) << ","
                << core::snmpVersionToString(r.version) << ","
                << r.responseTimeMs() << ","
                << (r.success ? "true" : "false") << ","
                << vb.oid << ","
                << core::snmpDataTypeToString(vb.type) << ","
                << "\"" << vb.value << "\"\n";
        }
    }

    return oss.str();
}

std::string SnmpRepository::serializeCredentials(const core::SnmpCredentials& creds) {
    nlohmann::json j;

    if (auto* v2c = std::get_if<core::SnmpV2cCredentials>(&creds)) {
        j["type"] = "v2c";
        j["community"] = v2c->community;
    } else if (auto* v3 = std::get_if<core::SnmpV3Credentials>(&creds)) {
        j["type"] = "v3";
        j["username"] = v3->username;
        j["security_level"] = static_cast<int>(v3->securityLevel);
        j["auth_protocol"] = static_cast<int>(v3->authProtocol);
        j["auth_password"] = v3->authPassword;  // Should be encrypted in production
        j["priv_protocol"] = static_cast<int>(v3->privProtocol);
        j["priv_password"] = v3->privPassword;  // Should be encrypted in production
        j["context_name"] = v3->contextName;
        j["context_engine_id"] = v3->contextEngineId;
    }

    return j.dump();
}

core::SnmpCredentials SnmpRepository::deserializeCredentials(
    const std::string& json, core::SnmpVersion version) {
    try {
        auto j = nlohmann::json::parse(json);

        std::string type = j.value("type", "v2c");

        if (type == "v3" || version == core::SnmpVersion::V3) {
            core::SnmpV3Credentials v3;
            v3.username = j.value("username", "");
            v3.securityLevel = static_cast<core::SnmpSecurityLevel>(
                j.value("security_level", static_cast<int>(core::SnmpSecurityLevel::NoAuthNoPriv)));
            v3.authProtocol = static_cast<core::SnmpAuthProtocol>(
                j.value("auth_protocol", static_cast<int>(core::SnmpAuthProtocol::None)));
            v3.authPassword = j.value("auth_password", "");
            v3.privProtocol = static_cast<core::SnmpPrivProtocol>(
                j.value("priv_protocol", static_cast<int>(core::SnmpPrivProtocol::None)));
            v3.privPassword = j.value("priv_password", "");
            v3.contextName = j.value("context_name", "");
            v3.contextEngineId = j.value("context_engine_id", "");
            return v3;
        } else {
            core::SnmpV2cCredentials v2c;
            v2c.community = j.value("community", "public");
            return v2c;
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to deserialize SNMP credentials: {}", e.what());
        return core::SnmpV2cCredentials{};  // Return default
    }
}

std::string SnmpRepository::serializeOids(const std::vector<std::string>& oids) {
    nlohmann::json j = oids;
    return j.dump();
}

std::vector<std::string> SnmpRepository::deserializeOids(const std::string& json) {
    try {
        auto j = nlohmann::json::parse(json);
        return j.get<std::vector<std::string>>();
    } catch (const std::exception& e) {
        spdlog::warn("Failed to deserialize OIDs: {}", e.what());
        return {};
    }
}

} // namespace netpulse::infra
