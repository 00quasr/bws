/**
 * @file PingResult.hpp
 * @brief Ping result types and statistics structures.
 *
 * This file defines the result of individual ping operations and
 * aggregated statistics for ping monitoring.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::core {

/**
 * @brief Result of a single ICMP ping operation.
 *
 * Contains all information about a ping attempt including timing,
 * success status, and any error information.
 */
struct PingResult {
    int64_t id{0};           ///< Unique identifier for the result
    int64_t hostId{0};       ///< ID of the host that was pinged
    std::chrono::system_clock::time_point timestamp; ///< When the ping was performed
    std::chrono::microseconds latency{0}; ///< Round-trip time in microseconds
    bool success{false};     ///< Whether the ping received a response
    std::optional<int> ttl;  ///< Time-to-live from the response (if available)
    std::string errorMessage; ///< Error message if the ping failed

    /**
     * @brief Converts the latency to milliseconds.
     * @return Latency as a floating-point number of milliseconds.
     */
    [[nodiscard]] double latencyMs() const {
        return static_cast<double>(latency.count()) / 1000.0;
    }

    bool operator==(const PingResult& other) const = default;
};

/**
 * @brief Aggregated statistics for ping results over time.
 *
 * Provides summary statistics for all ping results collected for a host.
 */
struct PingStatistics {
    int64_t hostId{0};        ///< ID of the host these statistics are for
    int totalPings{0};        ///< Total number of ping attempts
    int successfulPings{0};   ///< Number of successful pings
    std::chrono::microseconds minLatency{0}; ///< Minimum observed latency
    std::chrono::microseconds maxLatency{0}; ///< Maximum observed latency
    std::chrono::microseconds avgLatency{0}; ///< Average latency
    std::chrono::microseconds jitter{0};     ///< Latency variation (jitter)
    double packetLossPercent{0.0}; ///< Percentage of pings that failed

    /**
     * @brief Calculates the success rate as a percentage.
     * @return Percentage of successful pings (0-100).
     */
    [[nodiscard]] double successRate() const {
        return totalPings > 0 ? (static_cast<double>(successfulPings) / totalPings) * 100.0 : 0.0;
    }
};

} // namespace netpulse::core
