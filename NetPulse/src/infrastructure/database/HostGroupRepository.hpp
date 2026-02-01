#pragma once

#include "core/types/HostGroup.hpp"
#include "infrastructure/database/Database.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Repository for HostGroup entity persistence operations.
 *
 * Provides CRUD operations and queries for HostGroup entities.
 * Supports hierarchical groups with parent-child relationships.
 */
class HostGroupRepository {
public:
    /**
     * @brief Constructs a HostGroupRepository with the given database.
     * @param db Shared pointer to the Database instance.
     */
    explicit HostGroupRepository(std::shared_ptr<Database> db);

    /**
     * @brief Inserts a new host group into the database.
     * @param group HostGroup entity to insert.
     * @return ID of the newly inserted group.
     */
    int64_t insert(const core::HostGroup& group);

    /**
     * @brief Updates an existing host group in the database.
     * @param group HostGroup entity with updated values (id must be set).
     */
    void update(const core::HostGroup& group);

    /**
     * @brief Removes a host group from the database.
     * @param id ID of the group to remove.
     */
    void remove(int64_t id);

    /**
     * @brief Finds a host group by its ID.
     * @param id ID of the group to find.
     * @return HostGroup if found, nullopt otherwise.
     */
    std::optional<core::HostGroup> findById(int64_t id);

    /**
     * @brief Retrieves all host groups from the database.
     * @return Vector of all HostGroup entities.
     */
    std::vector<core::HostGroup> findAll();

    /**
     * @brief Finds groups with the specified parent.
     * @param parentId ID of the parent group, or nullopt for root groups.
     * @return Vector of child groups.
     */
    std::vector<core::HostGroup> findByParentId(std::optional<int64_t> parentId);

    /**
     * @brief Finds all root-level groups (no parent).
     * @return Vector of root groups.
     */
    std::vector<core::HostGroup> findRootGroups();

    /**
     * @brief Returns the total count of groups in the database.
     * @return Number of groups.
     */
    int count();

private:
    core::HostGroup rowToHostGroup(Statement& stmt);
    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
