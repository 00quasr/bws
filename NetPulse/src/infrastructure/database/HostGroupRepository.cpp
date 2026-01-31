#include "infrastructure/database/HostGroupRepository.hpp"

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

HostGroupRepository::HostGroupRepository(std::shared_ptr<Database> db) : db_(std::move(db)) {}

int64_t HostGroupRepository::insert(const core::HostGroup& group) {
    auto stmt = db_->prepare(R"(
        INSERT INTO host_groups (name, description, parent_id, created_at)
        VALUES (?, ?, ?, ?)
    )");

    stmt.bind(1, group.name);
    stmt.bind(2, group.description);
    if (group.parentId) {
        stmt.bind(3, *group.parentId);
    } else {
        stmt.bindNull(3);
    }
    stmt.bind(4, timePointToString(group.createdAt));

    stmt.step();
    auto id = db_->lastInsertRowId();
    spdlog::debug("Inserted host group with id: {}", id);
    return id;
}

void HostGroupRepository::update(const core::HostGroup& group) {
    auto stmt = db_->prepare(R"(
        UPDATE host_groups SET name = ?, description = ?, parent_id = ?
        WHERE id = ?
    )");

    stmt.bind(1, group.name);
    stmt.bind(2, group.description);
    if (group.parentId) {
        stmt.bind(3, *group.parentId);
    } else {
        stmt.bindNull(3);
    }
    stmt.bind(4, group.id);

    stmt.step();
    spdlog::debug("Updated host group: {}", group.id);
}

void HostGroupRepository::remove(int64_t id) {
    auto stmt = db_->prepare("DELETE FROM host_groups WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
    spdlog::debug("Removed host group: {}", id);
}

std::optional<core::HostGroup> HostGroupRepository::findById(int64_t id) {
    auto stmt = db_->prepare("SELECT * FROM host_groups WHERE id = ?");
    stmt.bind(1, id);

    if (stmt.step()) {
        return rowToHostGroup(stmt);
    }
    return std::nullopt;
}

std::vector<core::HostGroup> HostGroupRepository::findAll() {
    std::vector<core::HostGroup> groups;
    auto stmt = db_->prepare("SELECT * FROM host_groups ORDER BY name");

    while (stmt.step()) {
        groups.push_back(rowToHostGroup(stmt));
    }
    return groups;
}

std::vector<core::HostGroup> HostGroupRepository::findByParentId(std::optional<int64_t> parentId) {
    std::vector<core::HostGroup> groups;
    Statement stmt = parentId
        ? db_->prepare("SELECT * FROM host_groups WHERE parent_id = ? ORDER BY name")
        : db_->prepare("SELECT * FROM host_groups WHERE parent_id IS NULL ORDER BY name");

    if (parentId) {
        stmt.bind(1, *parentId);
    }

    while (stmt.step()) {
        groups.push_back(rowToHostGroup(stmt));
    }
    return groups;
}

std::vector<core::HostGroup> HostGroupRepository::findRootGroups() {
    return findByParentId(std::nullopt);
}

int HostGroupRepository::count() {
    auto stmt = db_->prepare("SELECT COUNT(*) FROM host_groups");
    stmt.step();
    return stmt.columnInt(0);
}

core::HostGroup HostGroupRepository::rowToHostGroup(Statement& stmt) {
    core::HostGroup group;
    group.id = stmt.columnInt64(0);
    group.name = stmt.columnText(1);
    group.description = stmt.columnText(2);
    if (!stmt.columnIsNull(3)) {
        group.parentId = stmt.columnInt64(3);
    }
    group.createdAt = stringToTimePoint(stmt.columnText(4));
    return group;
}

} // namespace netpulse::infra
