#include "infrastructure/database/ScheduledScanRepository.hpp"

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

ScheduledScanRepository::ScheduledScanRepository(std::shared_ptr<Database> db) : db_(std::move(db)) {}

std::string ScheduledScanRepository::portRangeToString(core::PortRange range) {
    switch (range) {
        case core::PortRange::Common:
            return "Common";
        case core::PortRange::Web:
            return "Web";
        case core::PortRange::Database:
            return "Database";
        case core::PortRange::All:
            return "All";
        case core::PortRange::Custom:
            return "Custom";
    }
    return "Common";
}

core::PortRange ScheduledScanRepository::portRangeFromString(const std::string& str) {
    if (str == "Web") return core::PortRange::Web;
    if (str == "Database") return core::PortRange::Database;
    if (str == "All") return core::PortRange::All;
    if (str == "Custom") return core::PortRange::Custom;
    return core::PortRange::Common;
}

std::string ScheduledScanRepository::portsToString(const std::vector<uint16_t>& ports) {
    std::ostringstream oss;
    for (size_t i = 0; i < ports.size(); ++i) {
        if (i > 0) oss << ",";
        oss << ports[i];
    }
    return oss.str();
}

std::vector<uint16_t> ScheduledScanRepository::portsFromString(const std::string& str) {
    std::vector<uint16_t> ports;
    if (str.empty()) return ports;

    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        try {
            ports.push_back(static_cast<uint16_t>(std::stoi(token)));
        } catch (...) {
        }
    }
    return ports;
}

int64_t ScheduledScanRepository::insertSchedule(const core::ScheduledScanConfig& config) {
    auto stmt = db_->prepare(R"(
        INSERT INTO scheduled_scans (name, target_address, port_range, custom_ports,
                                     interval_minutes, enabled, notify_on_changes, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");

    stmt.bind(1, config.name);
    stmt.bind(2, config.targetAddress);
    stmt.bind(3, portRangeToString(config.portRange));
    stmt.bind(4, portsToString(config.customPorts));
    stmt.bind(5, config.intervalMinutes);
    stmt.bind(6, config.enabled ? 1 : 0);
    stmt.bind(7, config.notifyOnChanges ? 1 : 0);
    stmt.bind(8, timePointToString(std::chrono::system_clock::now()));

    stmt.step();
    auto id = db_->lastInsertRowId();
    spdlog::debug("Inserted scheduled scan: {} (ID: {})", config.name, id);
    return id;
}

void ScheduledScanRepository::updateSchedule(const core::ScheduledScanConfig& config) {
    auto stmt = db_->prepare(R"(
        UPDATE scheduled_scans SET
            name = ?, target_address = ?, port_range = ?, custom_ports = ?,
            interval_minutes = ?, enabled = ?, notify_on_changes = ?
        WHERE id = ?
    )");

    stmt.bind(1, config.name);
    stmt.bind(2, config.targetAddress);
    stmt.bind(3, portRangeToString(config.portRange));
    stmt.bind(4, portsToString(config.customPorts));
    stmt.bind(5, config.intervalMinutes);
    stmt.bind(6, config.enabled ? 1 : 0);
    stmt.bind(7, config.notifyOnChanges ? 1 : 0);
    stmt.bind(8, config.id);

    stmt.step();
    spdlog::debug("Updated scheduled scan: {} (ID: {})", config.name, config.id);
}

void ScheduledScanRepository::removeSchedule(int64_t id) {
    auto stmt = db_->prepare("DELETE FROM scheduled_scans WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
    spdlog::debug("Removed scheduled scan: {}", id);
}

std::optional<core::ScheduledScanConfig> ScheduledScanRepository::findById(int64_t id) {
    auto stmt = db_->prepare(R"(
        SELECT id, name, target_address, port_range, custom_ports, interval_minutes,
               enabled, notify_on_changes, created_at, last_run_at
        FROM scheduled_scans WHERE id = ?
    )");

    stmt.bind(1, id);

    if (stmt.step()) {
        core::ScheduledScanConfig config;
        config.id = stmt.columnInt64(0);
        config.name = stmt.columnText(1);
        config.targetAddress = stmt.columnText(2);
        config.portRange = portRangeFromString(stmt.columnText(3));
        config.customPorts = portsFromString(stmt.columnText(4));
        config.intervalMinutes = stmt.columnInt(5);
        config.enabled = stmt.columnInt(6) != 0;
        config.notifyOnChanges = stmt.columnInt(7) != 0;
        config.createdAt = stringToTimePoint(stmt.columnText(8));
        if (!stmt.columnIsNull(9)) {
            config.lastRunAt = stringToTimePoint(stmt.columnText(9));
        }
        return config;
    }

    return std::nullopt;
}

std::vector<core::ScheduledScanConfig> ScheduledScanRepository::findAll() {
    std::vector<core::ScheduledScanConfig> results;
    auto stmt = db_->prepare(R"(
        SELECT id, name, target_address, port_range, custom_ports, interval_minutes,
               enabled, notify_on_changes, created_at, last_run_at
        FROM scheduled_scans ORDER BY name
    )");

    while (stmt.step()) {
        core::ScheduledScanConfig config;
        config.id = stmt.columnInt64(0);
        config.name = stmt.columnText(1);
        config.targetAddress = stmt.columnText(2);
        config.portRange = portRangeFromString(stmt.columnText(3));
        config.customPorts = portsFromString(stmt.columnText(4));
        config.intervalMinutes = stmt.columnInt(5);
        config.enabled = stmt.columnInt(6) != 0;
        config.notifyOnChanges = stmt.columnInt(7) != 0;
        config.createdAt = stringToTimePoint(stmt.columnText(8));
        if (!stmt.columnIsNull(9)) {
            config.lastRunAt = stringToTimePoint(stmt.columnText(9));
        }
        results.push_back(config);
    }

    return results;
}

std::vector<core::ScheduledScanConfig> ScheduledScanRepository::findEnabled() {
    std::vector<core::ScheduledScanConfig> results;
    auto stmt = db_->prepare(R"(
        SELECT id, name, target_address, port_range, custom_ports, interval_minutes,
               enabled, notify_on_changes, created_at, last_run_at
        FROM scheduled_scans WHERE enabled = 1 ORDER BY name
    )");

    while (stmt.step()) {
        core::ScheduledScanConfig config;
        config.id = stmt.columnInt64(0);
        config.name = stmt.columnText(1);
        config.targetAddress = stmt.columnText(2);
        config.portRange = portRangeFromString(stmt.columnText(3));
        config.customPorts = portsFromString(stmt.columnText(4));
        config.intervalMinutes = stmt.columnInt(5);
        config.enabled = stmt.columnInt(6) != 0;
        config.notifyOnChanges = stmt.columnInt(7) != 0;
        config.createdAt = stringToTimePoint(stmt.columnText(8));
        if (!stmt.columnIsNull(9)) {
            config.lastRunAt = stringToTimePoint(stmt.columnText(9));
        }
        results.push_back(config);
    }

    return results;
}

void ScheduledScanRepository::updateLastRunAt(int64_t id,
                                               std::chrono::system_clock::time_point time) {
    auto stmt = db_->prepare("UPDATE scheduled_scans SET last_run_at = ? WHERE id = ?");
    stmt.bind(1, timePointToString(time));
    stmt.bind(2, id);
    stmt.step();
}

void ScheduledScanRepository::setEnabled(int64_t id, bool enabled) {
    auto stmt = db_->prepare("UPDATE scheduled_scans SET enabled = ? WHERE id = ?");
    stmt.bind(1, enabled ? 1 : 0);
    stmt.bind(2, id);
    stmt.step();
}

int64_t ScheduledScanRepository::insertDiff(const core::PortScanDiff& diff, int64_t scheduleId) {
    auto stmt = db_->prepare(R"(
        INSERT INTO port_scan_diffs (schedule_id, target_address, previous_scan_time,
                                     current_scan_time, changes_json, total_ports_scanned,
                                     open_ports_before, open_ports_after)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");

    stmt.bind(1, scheduleId);
    stmt.bind(2, diff.targetAddress);
    stmt.bind(3, timePointToString(diff.previousScanTime));
    stmt.bind(4, timePointToString(diff.currentScanTime));
    stmt.bind(5, exportDiffToJson(diff));
    stmt.bind(6, diff.totalPortsScanned);
    stmt.bind(7, diff.openPortsBefore);
    stmt.bind(8, diff.openPortsAfter);

    stmt.step();
    return db_->lastInsertRowId();
}

std::vector<core::PortScanDiff> ScheduledScanRepository::getDiffs(int64_t scheduleId, int limit) {
    std::vector<core::PortScanDiff> results;
    auto stmt = db_->prepare(R"(
        SELECT id, target_address, previous_scan_time, current_scan_time, changes_json,
               total_ports_scanned, open_ports_before, open_ports_after
        FROM port_scan_diffs WHERE schedule_id = ?
        ORDER BY current_scan_time DESC LIMIT ?
    )");

    stmt.bind(1, scheduleId);
    stmt.bind(2, limit);

    while (stmt.step()) {
        core::PortScanDiff diff;
        diff.id = stmt.columnInt64(0);
        diff.targetAddress = stmt.columnText(1);
        diff.previousScanTime = stringToTimePoint(stmt.columnText(2));
        diff.currentScanTime = stringToTimePoint(stmt.columnText(3));

        auto changesJson = stmt.columnText(4);
        if (!changesJson.empty()) {
            try {
                auto j = nlohmann::json::parse(changesJson);
                if (j.contains("changes") && j["changes"].is_array()) {
                    for (const auto& changeObj : j["changes"]) {
                        core::PortChange change;
                        change.port = static_cast<uint16_t>(changeObj.value("port", 0));
                        change.changeType =
                            core::PortChange::changeTypeFromString(changeObj.value("type", ""));
                        change.previousState =
                            core::PortScanResult::stateFromString(changeObj.value("prev_state", ""));
                        change.currentState =
                            core::PortScanResult::stateFromString(changeObj.value("curr_state", ""));
                        change.serviceName = changeObj.value("service", "");
                        diff.changes.push_back(change);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse diff changes JSON: {}", e.what());
            }
        }

        diff.totalPortsScanned = stmt.columnInt(5);
        diff.openPortsBefore = stmt.columnInt(6);
        diff.openPortsAfter = stmt.columnInt(7);
        results.push_back(diff);
    }

    return results;
}

std::vector<core::PortScanDiff> ScheduledScanRepository::getDiffsByAddress(const std::string& address,
                                                                            int limit) {
    std::vector<core::PortScanDiff> results;
    auto stmt = db_->prepare(R"(
        SELECT id, target_address, previous_scan_time, current_scan_time, changes_json,
               total_ports_scanned, open_ports_before, open_ports_after
        FROM port_scan_diffs WHERE target_address = ?
        ORDER BY current_scan_time DESC LIMIT ?
    )");

    stmt.bind(1, address);
    stmt.bind(2, limit);

    while (stmt.step()) {
        core::PortScanDiff diff;
        diff.id = stmt.columnInt64(0);
        diff.targetAddress = stmt.columnText(1);
        diff.previousScanTime = stringToTimePoint(stmt.columnText(2));
        diff.currentScanTime = stringToTimePoint(stmt.columnText(3));

        auto changesJson = stmt.columnText(4);
        if (!changesJson.empty()) {
            try {
                auto j = nlohmann::json::parse(changesJson);
                if (j.contains("changes") && j["changes"].is_array()) {
                    for (const auto& changeObj : j["changes"]) {
                        core::PortChange change;
                        change.port = static_cast<uint16_t>(changeObj.value("port", 0));
                        change.changeType =
                            core::PortChange::changeTypeFromString(changeObj.value("type", ""));
                        change.previousState =
                            core::PortScanResult::stateFromString(changeObj.value("prev_state", ""));
                        change.currentState =
                            core::PortScanResult::stateFromString(changeObj.value("curr_state", ""));
                        change.serviceName = changeObj.value("service", "");
                        diff.changes.push_back(change);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse diff changes JSON: {}", e.what());
            }
        }

        diff.totalPortsScanned = stmt.columnInt(5);
        diff.openPortsBefore = stmt.columnInt(6);
        diff.openPortsAfter = stmt.columnInt(7);
        results.push_back(diff);
    }

    return results;
}

void ScheduledScanRepository::cleanupOldDiffs(std::chrono::hours maxAge) {
    auto cutoff = std::chrono::system_clock::now() - maxAge;
    auto stmt = db_->prepare("DELETE FROM port_scan_diffs WHERE current_scan_time < ?");
    stmt.bind(1, timePointToString(cutoff));
    stmt.step();
    spdlog::info("Cleaned up port scan diffs older than {} hours", maxAge.count());
}

std::string ScheduledScanRepository::exportDiffToJson(const core::PortScanDiff& diff) {
    nlohmann::json j;
    j["target_address"] = diff.targetAddress;
    j["previous_scan_time"] = timePointToString(diff.previousScanTime);
    j["current_scan_time"] = timePointToString(diff.currentScanTime);
    j["total_ports_scanned"] = diff.totalPortsScanned;
    j["open_ports_before"] = diff.openPortsBefore;
    j["open_ports_after"] = diff.openPortsAfter;

    j["changes"] = nlohmann::json::array();
    for (const auto& change : diff.changes) {
        nlohmann::json changeObj;
        changeObj["port"] = change.port;
        changeObj["type"] = change.changeTypeToString();
        changeObj["prev_state"] = core::PortScanResult::portStateToString(change.previousState);
        changeObj["curr_state"] = core::PortScanResult::portStateToString(change.currentState);
        changeObj["service"] = change.serviceName;
        j["changes"].push_back(changeObj);
    }

    return j.dump();
}

} // namespace netpulse::infra
