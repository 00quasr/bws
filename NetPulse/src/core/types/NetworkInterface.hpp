/**
 * @file NetworkInterface.hpp
 * @brief Network interface types and enumeration utilities.
 *
 * This file defines structures for representing network interfaces and their
 * statistics, along with a utility class for enumerating system interfaces.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace netpulse::core {

/**
 * @brief Statistics for a network interface.
 *
 * Contains byte and packet counters for monitoring network interface activity.
 */
struct NetworkInterfaceStats {
    uint64_t bytesReceived{0};   ///< Total bytes received on the interface
    uint64_t bytesSent{0};       ///< Total bytes sent on the interface
    uint64_t packetsReceived{0}; ///< Total packets received
    uint64_t packetsSent{0};     ///< Total packets sent
    uint64_t errorsIn{0};        ///< Number of receive errors
    uint64_t errorsOut{0};       ///< Number of transmit errors
    uint64_t dropsIn{0};         ///< Number of dropped incoming packets
    uint64_t dropsOut{0};        ///< Number of dropped outgoing packets
};

/**
 * @brief Represents a system network interface.
 *
 * Contains identifying information and current statistics for a network
 * interface on the local system.
 */
struct NetworkInterface {
    std::string name;            ///< System name of the interface (e.g., "eth0", "en0")
    std::string displayName;     ///< Human-readable display name
    std::string ipAddress;       ///< IPv4 address assigned to the interface
    std::string macAddress;      ///< MAC address of the interface
    bool isUp{false};            ///< Whether the interface is currently up
    bool isLoopback{false};      ///< Whether this is a loopback interface
    NetworkInterfaceStats stats; ///< Current interface statistics

    /**
     * @brief Formats bytes received as a human-readable string.
     * @return Formatted string (e.g., "1.5 GB", "256 MB").
     */
    [[nodiscard]] std::string formatBytesReceived() const;

    /**
     * @brief Formats bytes sent as a human-readable string.
     * @return Formatted string (e.g., "1.5 GB", "256 MB").
     */
    [[nodiscard]] std::string formatBytesSent() const;

    bool operator==(const NetworkInterface& other) const = default;
};

/**
 * @brief Utility class for enumerating network interfaces.
 *
 * Provides static methods to discover and query network interfaces
 * on the local system.
 */
class NetworkInterfaceEnumerator {
public:
    /**
     * @brief Enumerates all network interfaces on the system.
     * @return Vector of NetworkInterface objects representing all interfaces.
     */
    static std::vector<NetworkInterface> enumerate();

    /**
     * @brief Gets current statistics for a specific interface.
     * @param interfaceName The system name of the interface (e.g., "eth0").
     * @return Current statistics for the interface.
     */
    static NetworkInterfaceStats getStats(const std::string& interfaceName);
};

} // namespace netpulse::core
