#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::core {

struct PingResult {
    int64_t id{0};
    int64_t hostId{0};
    std::chrono::system_clock::time_point timestamp;
    std::chrono::microseconds latency{0};
    bool success{false};
    std::optional<int> ttl;
    std::string errorMessage;

    [[nodiscard]] double latencyMs() const {
        return static_cast<double>(latency.count()) / 1000.0;
    }

    bool operator==(const PingResult& other) const = default;
};

struct PingStatistics {
    int64_t hostId{0};
    int totalPings{0};
    int successfulPings{0};
    std::chrono::microseconds minLatency{0};
    std::chrono::microseconds maxLatency{0};
    std::chrono::microseconds avgLatency{0};
    std::chrono::microseconds jitter{0};
    double packetLossPercent{0.0};

    [[nodiscard]] double successRate() const {
        return totalPings > 0 ? (static_cast<double>(successfulPings) / totalPings) * 100.0 : 0.0;
    }
};

} // namespace netpulse::core
