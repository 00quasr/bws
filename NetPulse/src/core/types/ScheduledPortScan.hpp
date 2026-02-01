/**
 * @file ScheduledPortScan.hpp
 * @brief Types for scheduled port scanning and change detection.
 *
 * This file defines structures for scheduling periodic port scans and
 * tracking changes in port states between scans.
 */

#pragma once

#include "core/types/PortScanResult.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace netpulse::core {

/**
 * @brief Types of port state changes detected between scans.
 */
enum class PortChangeType : int {
    NewOpen = 0,     ///< Port was not open before, now open
    NewClosed = 1,   ///< Port was open before, now closed
    StateChanged = 2 ///< Port state changed (e.g., filtered to closed)
};

/**
 * @brief Represents a change in a port's state between scans.
 *
 * Used to track when ports open, close, or change their state between
 * scheduled scan runs.
 */
struct PortChange {
    uint16_t port{0};                              ///< Port number that changed
    PortChangeType changeType{PortChangeType::StateChanged}; ///< Type of change
    PortState previousState{PortState::Unknown};   ///< State in previous scan
    PortState currentState{PortState::Unknown};    ///< State in current scan
    std::string serviceName;                       ///< Service running on the port (if known)

    /**
     * @brief Converts the change type to a human-readable string.
     * @return String representation (e.g., "NewOpen", "NewClosed").
     */
    [[nodiscard]] std::string changeTypeToString() const;

    /**
     * @brief Parses a string to get the corresponding PortChangeType.
     * @param str The string to parse.
     * @return The corresponding PortChangeType enum value.
     */
    static PortChangeType changeTypeFromString(const std::string& str);

    bool operator==(const PortChange& other) const = default;
};

/**
 * @brief Difference between two port scan results.
 *
 * Captures all the changes detected when comparing a new scan to a previous scan.
 */
struct PortScanDiff {
    int64_t id{0};                 ///< Unique identifier for the diff
    std::string targetAddress;     ///< Target that was scanned
    std::chrono::system_clock::time_point previousScanTime; ///< When previous scan ran
    std::chrono::system_clock::time_point currentScanTime;  ///< When current scan ran
    std::vector<PortChange> changes; ///< List of all detected changes
    int totalPortsScanned{0};      ///< Total ports scanned in this comparison
    int openPortsBefore{0};        ///< Number of open ports in previous scan
    int openPortsAfter{0};         ///< Number of open ports in current scan

    /**
     * @brief Checks if any changes were detected.
     * @return True if there are any port state changes.
     */
    [[nodiscard]] bool hasChanges() const { return !changes.empty(); }

    /**
     * @brief Counts the number of newly opened ports.
     * @return Number of ports that changed from non-open to open.
     */
    [[nodiscard]] int newOpenPorts() const;

    /**
     * @brief Counts the number of newly closed ports.
     * @return Number of ports that changed from open to non-open.
     */
    [[nodiscard]] int newClosedPorts() const;

    bool operator==(const PortScanDiff& other) const = default;
};

/**
 * @brief Configuration for a scheduled port scan.
 *
 * Defines when and how periodic port scans should be executed.
 */
struct ScheduledScanConfig {
    int64_t id{0};                   ///< Unique identifier for the schedule
    std::string name;                ///< Human-readable name for the schedule
    std::string targetAddress;       ///< Target IP address or hostname
    PortRange portRange{PortRange::Common}; ///< Port range to scan
    std::vector<uint16_t> customPorts; ///< Custom ports (when range is Custom)
    int intervalMinutes{60};         ///< Interval between scans in minutes
    bool enabled{true};              ///< Whether the schedule is active
    bool notifyOnChanges{true};      ///< Whether to send notifications on changes
    std::chrono::system_clock::time_point createdAt; ///< When the schedule was created
    std::optional<std::chrono::system_clock::time_point> lastRunAt;  ///< Last execution time
    std::optional<std::chrono::system_clock::time_point> nextRunAt;  ///< Next scheduled execution

    /**
     * @brief Validates the schedule configuration.
     * @return True if the schedule has valid configuration.
     */
    [[nodiscard]] bool isValid() const;

    /**
     * @brief Converts this schedule to a PortScanConfig.
     * @return A PortScanConfig suitable for executing a scan.
     */
    [[nodiscard]] PortScanConfig toPortScanConfig() const;

    bool operator==(const ScheduledScanConfig& other) const = default;
};

} // namespace netpulse::core
