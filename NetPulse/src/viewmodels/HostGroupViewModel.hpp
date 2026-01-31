#pragma once

#include "core/types/Host.hpp"
#include "core/types/HostGroup.hpp"
#include "infrastructure/database/HostGroupRepository.hpp"
#include "infrastructure/database/HostRepository.hpp"

#include <QObject>
#include <memory>
#include <optional>

namespace netpulse::viewmodels {

class HostGroupViewModel : public QObject {
    Q_OBJECT

public:
    explicit HostGroupViewModel(std::shared_ptr<infra::Database> db, QObject* parent = nullptr);

    // Group CRUD operations
    int64_t addGroup(const std::string& name, const std::string& description = "",
                     std::optional<int64_t> parentId = std::nullopt);
    void updateGroup(const core::HostGroup& group);
    void removeGroup(int64_t id);

    std::optional<core::HostGroup> getGroup(int64_t id) const;
    std::vector<core::HostGroup> getAllGroups() const;
    std::vector<core::HostGroup> getRootGroups() const;
    std::vector<core::HostGroup> getChildGroups(int64_t parentId) const;

    // Host-group relationship
    void assignHostToGroup(int64_t hostId, std::optional<int64_t> groupId);
    std::vector<core::Host> getHostsInGroup(int64_t groupId) const;
    std::vector<core::Host> getUngroupedHosts() const;

signals:
    void groupAdded(int64_t groupId);
    void groupUpdated(int64_t groupId);
    void groupRemoved(int64_t groupId);
    void hostGroupChanged(int64_t hostId, std::optional<int64_t> groupId);

private:
    std::shared_ptr<infra::Database> db_;
    std::unique_ptr<infra::HostGroupRepository> groupRepo_;
    std::unique_ptr<infra::HostRepository> hostRepo_;
};

} // namespace netpulse::viewmodels
