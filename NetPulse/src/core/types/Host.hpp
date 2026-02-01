/**
 * @file Host.hpp
 * @brief Host definition and status types for the monitoring system.
 *
 * This file defines the Host structure which represents a monitored network
 * endpoint and its associated status enumeration.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::core {

/**
 * @brief Current status of a monitored host.
 */
enum class HostStatus : int {
    Unknown = 0, ///< Status has not been determined yet
    Up = 1,      ///< Host is reachable and responding normally
    Warning = 2, ///< Host is reachable but showing degraded performance
    Down = 3     ///< Host is unreachable
};

/**
 * @brief Represents a monitored network host.
 *
 * Contains all configuration and state information for a host that is
 * being monitored by the ping service.
 */
struct Host {
    int64_t id{0};                    ///< Unique identifier for the host
    std::string name;                 ///< Human-readable name for the host
    std::string address;              ///< IP address or hostname to monitor
    int pingIntervalSeconds{30};      ///< Interval between ping checks in seconds
    int warningThresholdMs{100};      ///< Latency (ms) above which status becomes Warning
    int criticalThresholdMs{500};     ///< Latency (ms) above which host is considered degraded
    HostStatus status{HostStatus::Unknown}; ///< Current status of the host
    bool enabled{true};               ///< Whether monitoring is enabled for this host
    std::optional<int64_t> groupId;   ///< Optional group ID for organizing hosts
    std::chrono::system_clock::time_point createdAt; ///< When the host was created
    std::optional<std::chrono::system_clock::time_point> lastChecked; ///< Last successful check time

    /**
     * @brief Validates the host configuration.
     * @return True if the host has valid configuration (non-empty name and address).
     */
    [[nodiscard]] bool isValid() const;

    /**
     * @brief Converts the host status to a human-readable string.
     * @return String representation of the status (e.g., "Up", "Down").
     */
    [[nodiscard]] std::string statusToString() const;

    /**
     * @brief Parses a string to get the corresponding HostStatus.
     * @param str The string to parse (e.g., "Up", "Down", "Warning").
     * @return The corresponding HostStatus enum value.
     */
    static HostStatus statusFromString(const std::string& str);

    bool operator==(const Host& other) const = default;
};

} // namespace netpulse::core
