/**
 * @file HostGroupViewModel.hpp
 * @brief ViewModel for host group management.
 *
 * This file defines the HostGroupViewModel class which provides operations
 * for organizing hosts into hierarchical groups in the MVVM architecture.
 */

#pragma once

#include "core/types/Host.hpp"
#include "core/types/HostGroup.hpp"
#include "infrastructure/database/HostGroupRepository.hpp"
#include "infrastructure/database/HostRepository.hpp"

#include <QObject>
#include <memory>
#include <optional>

namespace netpulse::viewmodels {

/**
 * @brief ViewModel for organizing hosts into groups.
 *
 * Provides CRUD operations for host groups including hierarchical (nested)
 * groups, and manages the assignment of hosts to groups. Emits signals
 * for UI synchronization when group data changes.
 */
class HostGroupViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a HostGroupViewModel.
     * @param db Shared pointer to the database connection.
     * @param parent Optional parent QObject for Qt ownership.
     */
    explicit HostGroupViewModel(std::shared_ptr<infra::Database> db, QObject* parent = nullptr);

    /**
     * @brief Creates a new host group.
     * @param name Display name for the group.
     * @param description Optional description of the group.
     * @param parentId Optional parent group ID for nested groups.
     * @return The ID of the newly created group.
     */
    int64_t addGroup(const std::string& name, const std::string& description = "",
                     std::optional<int64_t> parentId = std::nullopt);

    /**
     * @brief Updates an existing group's configuration.
     * @param group The group object with updated fields.
     */
    void updateGroup(const core::HostGroup& group);

    /**
     * @brief Removes a group from the system.
     * @param id ID of the group to remove.
     */
    void removeGroup(int64_t id);

    /**
     * @brief Gets a specific group by ID.
     * @param id ID of the group to retrieve.
     * @return The group if found, std::nullopt otherwise.
     */
    std::optional<core::HostGroup> getGroup(int64_t id) const;

    /**
     * @brief Gets all host groups.
     * @return Vector of all groups in the database.
     */
    std::vector<core::HostGroup> getAllGroups() const;

    /**
     * @brief Gets all top-level groups (no parent).
     * @return Vector of root groups.
     */
    std::vector<core::HostGroup> getRootGroups() const;

    /**
     * @brief Gets all child groups of a parent group.
     * @param parentId ID of the parent group.
     * @return Vector of child groups.
     */
    std::vector<core::HostGroup> getChildGroups(int64_t parentId) const;

    /**
     * @brief Assigns a host to a group or removes from current group.
     * @param hostId ID of the host to assign.
     * @param groupId Target group ID, or std::nullopt to ungroup.
     */
    void assignHostToGroup(int64_t hostId, std::optional<int64_t> groupId);

    /**
     * @brief Gets all hosts belonging to a specific group.
     * @param groupId ID of the group to query.
     * @return Vector of hosts in the group.
     */
    std::vector<core::Host> getHostsInGroup(int64_t groupId) const;

    /**
     * @brief Gets all hosts not assigned to any group.
     * @return Vector of ungrouped hosts.
     */
    std::vector<core::Host> getUngroupedHosts() const;

signals:
    /**
     * @brief Emitted when a new group is created.
     * @param groupId ID of the newly created group.
     */
    void groupAdded(int64_t groupId);

    /**
     * @brief Emitted when a group is updated.
     * @param groupId ID of the updated group.
     */
    void groupUpdated(int64_t groupId);

    /**
     * @brief Emitted when a group is removed.
     * @param groupId ID of the removed group.
     */
    void groupRemoved(int64_t groupId);

    /**
     * @brief Emitted when a host's group assignment changes.
     * @param hostId ID of the host whose group changed.
     * @param groupId New group ID, or std::nullopt if ungrouped.
     */
    void hostGroupChanged(int64_t hostId, std::optional<int64_t> groupId);

private:
    std::shared_ptr<infra::Database> db_;
    std::unique_ptr<infra::HostGroupRepository> groupRepo_;
    std::unique_ptr<infra::HostRepository> hostRepo_;
};

} // namespace netpulse::viewmodels
