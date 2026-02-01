/**
 * @file PortScanResult.hpp
 * @brief Port scanning types, results, and configuration structures.
 *
 * This file defines the types used for port scanning operations including
 * port states, scan results, scan configuration, and service detection.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace netpulse::core {

/**
 * @brief Possible states of a scanned port.
 */
enum class PortState : int {
    Unknown = 0,  ///< Port state could not be determined
    Open = 1,     ///< Port is accepting connections
    Closed = 2,   ///< Port is reachable but not accepting connections
    Filtered = 3  ///< Port is filtered by a firewall (no response)
};

/**
 * @brief Result of scanning a single port.
 *
 * Contains the state of the port and any detected service information.
 */
struct PortScanResult {
    int64_t id{0};               ///< Unique identifier for the result
    std::string targetAddress;   ///< Address that was scanned
    uint16_t port{0};            ///< Port number that was scanned
    PortState state{PortState::Unknown}; ///< Current state of the port
    std::string serviceName;     ///< Name of detected service (if known)
    std::chrono::system_clock::time_point scanTimestamp; ///< When the scan was performed

    /**
     * @brief Converts this result's port state to a string.
     * @return String representation of the state (e.g., "Open", "Closed").
     */
    [[nodiscard]] std::string stateToString() const;

    /**
     * @brief Converts a PortState enum to a string.
     * @param state The port state to convert.
     * @return String representation of the state.
     */
    static std::string portStateToString(PortState state);

    /**
     * @brief Parses a string to get the corresponding PortState.
     * @param str The string to parse (e.g., "Open", "Closed", "Filtered").
     * @return The corresponding PortState enum value.
     */
    static PortState stateFromString(const std::string& str);

    bool operator==(const PortScanResult& other) const = default;
};

/**
 * @brief Predefined port ranges for scanning.
 */
enum class PortRange {
    Common,   ///< Common service ports (e.g., 22, 80, 443, etc.)
    Web,      ///< Web-related ports (80, 443, 8080, etc.)
    Database, ///< Database ports (3306, 5432, 27017, etc.)
    All,      ///< All ports (1-65535)
    Custom    ///< Custom list of ports
};

/**
 * @brief Configuration for a port scan operation.
 *
 * Specifies the target, ports to scan, and scan parameters.
 */
struct PortScanConfig {
    std::string targetAddress;            ///< Target IP address or hostname
    PortRange range{PortRange::Common};   ///< Predefined port range to scan
    std::vector<uint16_t> customPorts;    ///< Custom ports (used when range is Custom)
    int maxConcurrency{100};              ///< Maximum concurrent connection attempts
    std::chrono::milliseconds timeout{1000}; ///< Timeout per port in milliseconds

    /**
     * @brief Gets the list of ports to scan based on the configuration.
     * @return Vector of port numbers to scan.
     */
    [[nodiscard]] std::vector<uint16_t> getPortsToScan() const;
};

/**
 * @brief Utility class for detecting services by port number.
 *
 * Provides static methods to identify common services running on standard ports.
 */
class ServiceDetector {
public:
    /**
     * @brief Detects the likely service running on a port.
     * @param port The port number to look up.
     * @return Service name if known, empty string otherwise.
     */
    static std::string detectService(uint16_t port);

    /**
     * @brief Gets the map of known port-to-service mappings.
     * @return Reference to the map of port numbers to service names.
     */
    static const std::unordered_map<uint16_t, std::string>& getKnownServices();
};

} // namespace netpulse::core
