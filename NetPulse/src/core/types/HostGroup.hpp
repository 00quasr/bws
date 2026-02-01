/**
 * @file HostGroup.hpp
 * @brief Host grouping structure for organizing monitored hosts.
 *
 * This file defines the HostGroup structure which allows logical grouping
 * of hosts for organizational purposes.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::core {

/**
 * @brief Represents a logical grouping of hosts.
 *
 * Host groups allow organizing monitored hosts into categories such as
 * by location, function, or department. Groups can be nested via parentId.
 */
struct HostGroup {
    int64_t id{0};                   ///< Unique identifier for the group
    std::string name;                ///< Name of the group
    std::string description;         ///< Optional description of the group's purpose
    std::optional<int64_t> parentId; ///< Parent group ID for nested grouping (null for root groups)
    std::chrono::system_clock::time_point createdAt; ///< When the group was created

    /**
     * @brief Validates the group configuration.
     * @return True if the group has a valid configuration (non-empty name).
     */
    [[nodiscard]] bool isValid() const;

    bool operator==(const HostGroup& other) const = default;
};

} // namespace netpulse::core
