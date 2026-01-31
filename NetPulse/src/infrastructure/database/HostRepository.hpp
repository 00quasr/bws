#pragma once

#include "core/types/Host.hpp"
#include "infrastructure/database/Database.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace netpulse::infra {

class HostRepository {
public:
    explicit HostRepository(std::shared_ptr<Database> db);

    int64_t insert(const core::Host& host);
    void update(const core::Host& host);
    void remove(int64_t id);

    std::optional<core::Host> findById(int64_t id);
    std::optional<core::Host> findByAddress(const std::string& address);
    std::vector<core::Host> findAll();
    std::vector<core::Host> findEnabled();
    std::vector<core::Host> findByGroupId(std::optional<int64_t> groupId);
    std::vector<core::Host> findUngrouped();

    void setHostGroup(int64_t hostId, std::optional<int64_t> groupId);

    void updateStatus(int64_t id, core::HostStatus status);
    void updateLastChecked(int64_t id);

    int count();

private:
    core::Host rowToHost(Statement& stmt);
    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
