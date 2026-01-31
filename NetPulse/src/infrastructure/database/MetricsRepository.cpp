#include "infrastructure/database/MetricsRepository.hpp"

#include <nlohmann/json.hpp>
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

MetricsRepository::MetricsRepository(std::shared_ptr<Database> db) : db_(std::move(db)) {}

int64_t MetricsRepository::insertPingResult(const core::PingResult& result) {
    auto stmt = db_->prepare(R"(
        INSERT INTO ping_results (host_id, timestamp, latency_us, success, ttl)
        VALUES (?, ?, ?, ?, ?)
    )");

    stmt.bind(1, result.hostId);
    stmt.bind(2, timePointToString(result.timestamp));
    stmt.bind(3, result.latency.count());
    stmt.bind(4, result.success ? 1 : 0);
    if (result.ttl) {
        stmt.bind(5, *result.ttl);
    } else {
        stmt.bindNull(5);
    }

    stmt.step();
    return db_->lastInsertRowId();
}

std::vector<core::PingResult> MetricsRepository::getPingResults(int64_t hostId, int limit) {
    std::vector<core::PingResult> results;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, timestamp, latency_us, success, ttl
        FROM ping_results WHERE host_id = ?
        ORDER BY timestamp DESC LIMIT ?
    )");

    stmt.bind(1, hostId);
    stmt.bind(2, limit);

    while (stmt.step()) {
        core::PingResult result;
        result.id = stmt.columnInt64(0);
        result.hostId = stmt.columnInt64(1);
        result.timestamp = stringToTimePoint(stmt.columnText(2));
        result.latency = std::chrono::microseconds(stmt.columnInt64(3));
        result.success = stmt.columnInt(4) != 0;
        if (!stmt.columnIsNull(5)) {
            result.ttl = stmt.columnInt(5);
        }
        results.push_back(result);
    }

    return results;
}

std::vector<core::PingResult> MetricsRepository::getPingResultsSince(
    int64_t hostId, std::chrono::system_clock::time_point since) {
    std::vector<core::PingResult> results;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, timestamp, latency_us, success, ttl
        FROM ping_results WHERE host_id = ? AND timestamp >= ?
        ORDER BY timestamp ASC
    )");

    stmt.bind(1, hostId);
    stmt.bind(2, timePointToString(since));

    while (stmt.step()) {
        core::PingResult result;
        result.id = stmt.columnInt64(0);
        result.hostId = stmt.columnInt64(1);
        result.timestamp = stringToTimePoint(stmt.columnText(2));
        result.latency = std::chrono::microseconds(stmt.columnInt64(3));
        result.success = stmt.columnInt(4) != 0;
        if (!stmt.columnIsNull(5)) {
            result.ttl = stmt.columnInt(5);
        }
        results.push_back(result);
    }

    return results;
}

core::PingStatistics MetricsRepository::getStatistics(int64_t hostId, int sampleCount) {
    core::PingStatistics stats;
    stats.hostId = hostId;

    auto stmt = db_->prepare(R"(
        SELECT
            COUNT(*) as total,
            SUM(CASE WHEN success = 1 THEN 1 ELSE 0 END) as successful,
            MIN(CASE WHEN success = 1 THEN latency_us END) as min_lat,
            MAX(CASE WHEN success = 1 THEN latency_us END) as max_lat,
            AVG(CASE WHEN success = 1 THEN latency_us END) as avg_lat
        FROM (
            SELECT * FROM ping_results WHERE host_id = ?
            ORDER BY timestamp DESC LIMIT ?
        )
    )");

    stmt.bind(1, hostId);
    stmt.bind(2, sampleCount);

    if (stmt.step()) {
        stats.totalPings = stmt.columnInt(0);
        stats.successfulPings = stmt.columnInt(1);

        if (!stmt.columnIsNull(2)) {
            stats.minLatency = std::chrono::microseconds(stmt.columnInt64(2));
        }
        if (!stmt.columnIsNull(3)) {
            stats.maxLatency = std::chrono::microseconds(stmt.columnInt64(3));
        }
        if (!stmt.columnIsNull(4)) {
            stats.avgLatency = std::chrono::microseconds(static_cast<int64_t>(stmt.columnDouble(4)));
        }

        if (stats.totalPings > 0) {
            stats.packetLossPercent =
                (1.0 - static_cast<double>(stats.successfulPings) / stats.totalPings) * 100.0;
        }
    }

    // Calculate jitter (average deviation from mean)
    if (stats.successfulPings > 1) {
        auto jitterStmt = db_->prepare(R"(
            SELECT AVG(ABS(latency_us - ?))
            FROM (
                SELECT latency_us FROM ping_results
                WHERE host_id = ? AND success = 1
                ORDER BY timestamp DESC LIMIT ?
            )
        )");

        jitterStmt.bind(1, stats.avgLatency.count());
        jitterStmt.bind(2, hostId);
        jitterStmt.bind(3, sampleCount);

        if (jitterStmt.step() && !jitterStmt.columnIsNull(0)) {
            stats.jitter =
                std::chrono::microseconds(static_cast<int64_t>(jitterStmt.columnDouble(0)));
        }
    }

    return stats;
}

void MetricsRepository::cleanupOldPingResults(std::chrono::hours maxAge) {
    auto cutoff = std::chrono::system_clock::now() - maxAge;
    auto stmt = db_->prepare("DELETE FROM ping_results WHERE timestamp < ?");
    stmt.bind(1, timePointToString(cutoff));
    stmt.step();
    spdlog::info("Cleaned up ping results older than {} hours", maxAge.count());
}

int64_t MetricsRepository::insertAlert(const core::Alert& alert) {
    auto stmt = db_->prepare(R"(
        INSERT INTO alerts (host_id, alert_type, severity, title, message, timestamp, acknowledged)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");

    stmt.bind(1, alert.hostId);
    stmt.bind(2, static_cast<int>(alert.type));
    stmt.bind(3, static_cast<int>(alert.severity));
    stmt.bind(4, alert.title);
    stmt.bind(5, alert.message);
    stmt.bind(6, timePointToString(alert.timestamp));
    stmt.bind(7, alert.acknowledged ? 1 : 0);

    stmt.step();
    return db_->lastInsertRowId();
}

std::vector<core::Alert> MetricsRepository::getAlerts(int limit) {
    std::vector<core::Alert> alerts;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, alert_type, severity, title, message, timestamp, acknowledged
        FROM alerts ORDER BY timestamp DESC LIMIT ?
    )");

    stmt.bind(1, limit);

    while (stmt.step()) {
        core::Alert alert;
        alert.id = stmt.columnInt64(0);
        alert.hostId = stmt.columnInt64(1);
        alert.type = static_cast<core::AlertType>(stmt.columnInt(2));
        alert.severity = static_cast<core::AlertSeverity>(stmt.columnInt(3));
        alert.title = stmt.columnText(4);
        alert.message = stmt.columnText(5);
        alert.timestamp = stringToTimePoint(stmt.columnText(6));
        alert.acknowledged = stmt.columnInt(7) != 0;
        alerts.push_back(alert);
    }

    return alerts;
}

std::vector<core::Alert> MetricsRepository::getUnacknowledgedAlerts() {
    std::vector<core::Alert> alerts;
    auto stmt = db_->prepare(R"(
        SELECT id, host_id, alert_type, severity, title, message, timestamp, acknowledged
        FROM alerts WHERE acknowledged = 0 ORDER BY timestamp DESC
    )");

    while (stmt.step()) {
        core::Alert alert;
        alert.id = stmt.columnInt64(0);
        alert.hostId = stmt.columnInt64(1);
        alert.type = static_cast<core::AlertType>(stmt.columnInt(2));
        alert.severity = static_cast<core::AlertSeverity>(stmt.columnInt(3));
        alert.title = stmt.columnText(4);
        alert.message = stmt.columnText(5);
        alert.timestamp = stringToTimePoint(stmt.columnText(6));
        alert.acknowledged = stmt.columnInt(7) != 0;
        alerts.push_back(alert);
    }

    return alerts;
}

void MetricsRepository::acknowledgeAlert(int64_t id) {
    auto stmt = db_->prepare("UPDATE alerts SET acknowledged = 1 WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
}

void MetricsRepository::acknowledgeAll() {
    db_->execute("UPDATE alerts SET acknowledged = 1 WHERE acknowledged = 0");
}

void MetricsRepository::cleanupOldAlerts(std::chrono::hours maxAge) {
    auto cutoff = std::chrono::system_clock::now() - maxAge;
    auto stmt = db_->prepare("DELETE FROM alerts WHERE timestamp < ?");
    stmt.bind(1, timePointToString(cutoff));
    stmt.step();
    spdlog::info("Cleaned up alerts older than {} hours", maxAge.count());
}

int64_t MetricsRepository::insertPortScanResult(const core::PortScanResult& result) {
    auto stmt = db_->prepare(R"(
        INSERT INTO port_scan_results (target_address, port, state, service_name, scan_timestamp)
        VALUES (?, ?, ?, ?, ?)
    )");

    stmt.bind(1, result.targetAddress);
    stmt.bind(2, static_cast<int>(result.port));
    stmt.bind(3, static_cast<int>(result.state));
    stmt.bind(4, result.serviceName);
    stmt.bind(5, timePointToString(result.scanTimestamp));

    stmt.step();
    return db_->lastInsertRowId();
}

void MetricsRepository::insertPortScanResults(const std::vector<core::PortScanResult>& results) {
    db_->transaction([&]() {
        for (const auto& result : results) {
            insertPortScanResult(result);
        }
    });
}

std::vector<core::PortScanResult> MetricsRepository::getPortScanResults(const std::string& address,
                                                                         int limit) {
    std::vector<core::PortScanResult> results;
    auto stmt = db_->prepare(R"(
        SELECT id, target_address, port, state, service_name, scan_timestamp
        FROM port_scan_results WHERE target_address = ?
        ORDER BY scan_timestamp DESC, port ASC LIMIT ?
    )");

    stmt.bind(1, address);
    stmt.bind(2, limit);

    while (stmt.step()) {
        core::PortScanResult result;
        result.id = stmt.columnInt64(0);
        result.targetAddress = stmt.columnText(1);
        result.port = static_cast<uint16_t>(stmt.columnInt(2));
        result.state = static_cast<core::PortState>(stmt.columnInt(3));
        result.serviceName = stmt.columnText(4);
        result.scanTimestamp = stringToTimePoint(stmt.columnText(5));
        results.push_back(result);
    }

    return results;
}

void MetricsRepository::cleanupOldPortScans(std::chrono::hours maxAge) {
    auto cutoff = std::chrono::system_clock::now() - maxAge;
    auto stmt = db_->prepare("DELETE FROM port_scan_results WHERE scan_timestamp < ?");
    stmt.bind(1, timePointToString(cutoff));
    stmt.step();
    spdlog::info("Cleaned up port scans older than {} hours", maxAge.count());
}

std::string MetricsRepository::exportToJson(int64_t hostId) {
    nlohmann::json j;

    auto results = getPingResults(hostId, 10000);
    j["host_id"] = hostId;
    j["export_time"] = timePointToString(std::chrono::system_clock::now());
    j["ping_results"] = nlohmann::json::array();

    for (const auto& r : results) {
        nlohmann::json entry;
        entry["timestamp"] = timePointToString(r.timestamp);
        entry["latency_ms"] = r.latencyMs();
        entry["success"] = r.success;
        if (r.ttl) {
            entry["ttl"] = *r.ttl;
        }
        j["ping_results"].push_back(entry);
    }

    auto stats = getStatistics(hostId);
    j["statistics"]["total_pings"] = stats.totalPings;
    j["statistics"]["successful_pings"] = stats.successfulPings;
    j["statistics"]["packet_loss_percent"] = stats.packetLossPercent;
    j["statistics"]["min_latency_ms"] = stats.minLatency.count() / 1000.0;
    j["statistics"]["max_latency_ms"] = stats.maxLatency.count() / 1000.0;
    j["statistics"]["avg_latency_ms"] = stats.avgLatency.count() / 1000.0;
    j["statistics"]["jitter_ms"] = stats.jitter.count() / 1000.0;

    return j.dump(2);
}

std::string MetricsRepository::exportToCsv(int64_t hostId) {
    std::ostringstream oss;
    oss << "timestamp,latency_ms,success,ttl\n";

    auto results = getPingResults(hostId, 10000);
    for (const auto& r : results) {
        oss << timePointToString(r.timestamp) << "," << r.latencyMs() << ","
            << (r.success ? "true" : "false") << ",";
        if (r.ttl) {
            oss << *r.ttl;
        }
        oss << "\n";
    }

    return oss.str();
}

} // namespace netpulse::infra
