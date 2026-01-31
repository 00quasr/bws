#pragma once

#include "core/types/HostGroup.hpp"
#include "infrastructure/database/Database.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace netpulse::infra {

class HostGroupRepository {
public:
    explicit HostGroupRepository(std::shared_ptr<Database> db);

    int64_t insert(const core::HostGroup& group);
    void update(const core::HostGroup& group);
    void remove(int64_t id);

    std::optional<core::HostGroup> findById(int64_t id);
    std::vector<core::HostGroup> findAll();
    std::vector<core::HostGroup> findByParentId(std::optional<int64_t> parentId);
    std::vector<core::HostGroup> findRootGroups();

    int count();

private:
    core::HostGroup rowToHostGroup(Statement& stmt);
    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
