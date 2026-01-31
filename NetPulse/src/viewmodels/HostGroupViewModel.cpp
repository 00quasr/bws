#include "viewmodels/HostGroupViewModel.hpp"

#include <spdlog/spdlog.h>

namespace netpulse::viewmodels {

HostGroupViewModel::HostGroupViewModel(std::shared_ptr<infra::Database> db, QObject* parent)
    : QObject(parent), db_(std::move(db)) {
    groupRepo_ = std::make_unique<infra::HostGroupRepository>(db_);
    hostRepo_ = std::make_unique<infra::HostRepository>(db_);
}

int64_t HostGroupViewModel::addGroup(const std::string& name, const std::string& description,
                                      std::optional<int64_t> parentId) {
    core::HostGroup group;
    group.name = name;
    group.description = description;
    group.parentId = parentId;
    group.createdAt = std::chrono::system_clock::now();

    int64_t id = groupRepo_->insert(group);
    spdlog::info("Added host group: {}", name);

    emit groupAdded(id);
    return id;
}

void HostGroupViewModel::updateGroup(const core::HostGroup& group) {
    groupRepo_->update(group);
    spdlog::info("Updated host group: {}", group.name);
    emit groupUpdated(group.id);
}

void HostGroupViewModel::removeGroup(int64_t id) {
    groupRepo_->remove(id);
    spdlog::info("Removed host group: {}", id);
    emit groupRemoved(id);
}

std::optional<core::HostGroup> HostGroupViewModel::getGroup(int64_t id) const {
    return groupRepo_->findById(id);
}

std::vector<core::HostGroup> HostGroupViewModel::getAllGroups() const {
    return groupRepo_->findAll();
}

std::vector<core::HostGroup> HostGroupViewModel::getRootGroups() const {
    return groupRepo_->findRootGroups();
}

std::vector<core::HostGroup> HostGroupViewModel::getChildGroups(int64_t parentId) const {
    return groupRepo_->findByParentId(parentId);
}

void HostGroupViewModel::assignHostToGroup(int64_t hostId, std::optional<int64_t> groupId) {
    hostRepo_->setHostGroup(hostId, groupId);
    spdlog::info("Assigned host {} to group {}", hostId, groupId.value_or(-1));
    emit hostGroupChanged(hostId, groupId);
}

std::vector<core::Host> HostGroupViewModel::getHostsInGroup(int64_t groupId) const {
    return hostRepo_->findByGroupId(groupId);
}

std::vector<core::Host> HostGroupViewModel::getUngroupedHosts() const {
    return hostRepo_->findUngrouped();
}

} // namespace netpulse::viewmodels
