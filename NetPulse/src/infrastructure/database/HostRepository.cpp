#include "infrastructure/database/HostRepository.hpp"

#include <spdlog/spdlog.h>

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

HostRepository::HostRepository(std::shared_ptr<Database> db) : db_(std::move(db)) {}

int64_t HostRepository::insert(const core::Host& host) {
    auto stmt = db_->prepare(R"(
        INSERT INTO hosts (name, address, ping_interval, warning_threshold_ms,
                          critical_threshold_ms, status, enabled, group_id, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    stmt.bind(1, host.name);
    stmt.bind(2, host.address);
    stmt.bind(3, host.pingIntervalSeconds);
    stmt.bind(4, host.warningThresholdMs);
    stmt.bind(5, host.criticalThresholdMs);
    stmt.bind(6, static_cast<int>(host.status));
    stmt.bind(7, host.enabled ? 1 : 0);
    if (host.groupId) {
        stmt.bind(8, *host.groupId);
    } else {
        stmt.bindNull(8);
    }
    stmt.bind(9, timePointToString(host.createdAt));

    stmt.step();
    auto id = db_->lastInsertRowId();
    spdlog::debug("Inserted host with id: {}", id);
    return id;
}

void HostRepository::update(const core::Host& host) {
    auto stmt = db_->prepare(R"(
        UPDATE hosts SET
            name = ?, address = ?, ping_interval = ?, warning_threshold_ms = ?,
            critical_threshold_ms = ?, status = ?, enabled = ?, group_id = ?
        WHERE id = ?
    )");

    stmt.bind(1, host.name);
    stmt.bind(2, host.address);
    stmt.bind(3, host.pingIntervalSeconds);
    stmt.bind(4, host.warningThresholdMs);
    stmt.bind(5, host.criticalThresholdMs);
    stmt.bind(6, static_cast<int>(host.status));
    stmt.bind(7, host.enabled ? 1 : 0);
    if (host.groupId) {
        stmt.bind(8, *host.groupId);
    } else {
        stmt.bindNull(8);
    }
    stmt.bind(9, host.id);

    stmt.step();
    spdlog::debug("Updated host: {}", host.id);
}

void HostRepository::remove(int64_t id) {
    auto stmt = db_->prepare("DELETE FROM hosts WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
    spdlog::debug("Removed host: {}", id);
}

std::optional<core::Host> HostRepository::findById(int64_t id) {
    auto stmt = db_->prepare("SELECT * FROM hosts WHERE id = ?");
    stmt.bind(1, id);

    if (stmt.step()) {
        return rowToHost(stmt);
    }
    return std::nullopt;
}

std::optional<core::Host> HostRepository::findByAddress(const std::string& address) {
    auto stmt = db_->prepare("SELECT * FROM hosts WHERE address = ?");
    stmt.bind(1, address);

    if (stmt.step()) {
        return rowToHost(stmt);
    }
    return std::nullopt;
}

std::vector<core::Host> HostRepository::findAll() {
    std::vector<core::Host> hosts;
    auto stmt = db_->prepare("SELECT * FROM hosts ORDER BY name");

    while (stmt.step()) {
        hosts.push_back(rowToHost(stmt));
    }
    return hosts;
}

std::vector<core::Host> HostRepository::findEnabled() {
    std::vector<core::Host> hosts;
    auto stmt = db_->prepare("SELECT * FROM hosts WHERE enabled = 1 ORDER BY name");

    while (stmt.step()) {
        hosts.push_back(rowToHost(stmt));
    }
    return hosts;
}

void HostRepository::updateStatus(int64_t id, core::HostStatus status) {
    auto stmt = db_->prepare("UPDATE hosts SET status = ? WHERE id = ?");
    stmt.bind(1, static_cast<int>(status));
    stmt.bind(2, id);
    stmt.step();
}

void HostRepository::updateLastChecked(int64_t id) {
    auto stmt = db_->prepare("UPDATE hosts SET last_checked = CURRENT_TIMESTAMP WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
}

int HostRepository::count() {
    auto stmt = db_->prepare("SELECT COUNT(*) FROM hosts");
    stmt.step();
    return stmt.columnInt(0);
}

std::vector<core::Host> HostRepository::findByGroupId(std::optional<int64_t> groupId) {
    std::vector<core::Host> hosts;
    Statement stmt = groupId
        ? db_->prepare("SELECT * FROM hosts WHERE group_id = ? ORDER BY name")
        : db_->prepare("SELECT * FROM hosts WHERE group_id IS NULL ORDER BY name");

    if (groupId) {
        stmt.bind(1, *groupId);
    }

    while (stmt.step()) {
        hosts.push_back(rowToHost(stmt));
    }
    return hosts;
}

std::vector<core::Host> HostRepository::findUngrouped() {
    return findByGroupId(std::nullopt);
}

void HostRepository::setHostGroup(int64_t hostId, std::optional<int64_t> groupId) {
    auto stmt = db_->prepare("UPDATE hosts SET group_id = ? WHERE id = ?");
    if (groupId) {
        stmt.bind(1, *groupId);
    } else {
        stmt.bindNull(1);
    }
    stmt.bind(2, hostId);
    stmt.step();
    spdlog::debug("Set host {} group to {}", hostId, groupId.value_or(-1));
}

core::Host HostRepository::rowToHost(Statement& stmt) {
    core::Host host;
    host.id = stmt.columnInt64(0);
    host.name = stmt.columnText(1);
    host.address = stmt.columnText(2);
    host.pingIntervalSeconds = stmt.columnInt(3);
    host.warningThresholdMs = stmt.columnInt(4);
    host.criticalThresholdMs = stmt.columnInt(5);
    host.status = static_cast<core::HostStatus>(stmt.columnInt(6));
    host.enabled = stmt.columnInt(7) != 0;
    host.createdAt = stringToTimePoint(stmt.columnText(8));

    if (!stmt.columnIsNull(9)) {
        host.lastChecked = stringToTimePoint(stmt.columnText(9));
    }

    // group_id is column 10 (added via ALTER TABLE)
    if (!stmt.columnIsNull(10)) {
        host.groupId = stmt.columnInt64(10);
    }

    return host;
}

} // namespace netpulse::infra
