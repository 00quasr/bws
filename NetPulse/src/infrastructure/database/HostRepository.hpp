#pragma once

#include "core/types/Host.hpp"
#include "infrastructure/database/Database.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Repository for Host entity persistence operations.
 *
 * Provides CRUD operations and queries for Host entities in the database.
 * Supports grouping, status tracking, and various lookup methods.
 */
class HostRepository {
public:
    /**
     * @brief Constructs a HostRepository with the given database.
     * @param db Shared pointer to the Database instance.
     */
    explicit HostRepository(std::shared_ptr<Database> db);

    /**
     * @brief Inserts a new host into the database.
     * @param host Host entity to insert.
     * @return ID of the newly inserted host.
     */
    int64_t insert(const core::Host& host);

    /**
     * @brief Updates an existing host in the database.
     * @param host Host entity with updated values (id must be set).
     */
    void update(const core::Host& host);

    /**
     * @brief Removes a host from the database.
     * @param id ID of the host to remove.
     */
    void remove(int64_t id);

    /**
     * @brief Finds a host by its ID.
     * @param id ID of the host to find.
     * @return Host if found, nullopt otherwise.
     */
    std::optional<core::Host> findById(int64_t id);

    /**
     * @brief Finds a host by its address.
     * @param address Hostname or IP address to search for.
     * @return Host if found, nullopt otherwise.
     */
    std::optional<core::Host> findByAddress(const std::string& address);

    /**
     * @brief Retrieves all hosts from the database.
     * @return Vector of all Host entities.
     */
    std::vector<core::Host> findAll();

    /**
     * @brief Retrieves all enabled hosts.
     * @return Vector of hosts where monitoring is enabled.
     */
    std::vector<core::Host> findEnabled();

    /**
     * @brief Finds hosts belonging to a specific group.
     * @param groupId ID of the group, or nullopt for ungrouped hosts.
     * @return Vector of hosts in the specified group.
     */
    std::vector<core::Host> findByGroupId(std::optional<int64_t> groupId);

    /**
     * @brief Finds hosts that are not assigned to any group.
     * @return Vector of ungrouped hosts.
     */
    std::vector<core::Host> findUngrouped();

    /**
     * @brief Assigns a host to a group.
     * @param hostId ID of the host to assign.
     * @param groupId ID of the group, or nullopt to ungroup.
     */
    void setHostGroup(int64_t hostId, std::optional<int64_t> groupId);

    /**
     * @brief Updates the status of a host.
     * @param id ID of the host.
     * @param status New status value.
     */
    void updateStatus(int64_t id, core::HostStatus status);

    /**
     * @brief Updates the last checked timestamp to current time.
     * @param id ID of the host.
     */
    void updateLastChecked(int64_t id);

    /**
     * @brief Returns the total count of hosts in the database.
     * @return Number of hosts.
     */
    int count();

private:
    core::Host rowToHost(Statement& stmt);
    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
