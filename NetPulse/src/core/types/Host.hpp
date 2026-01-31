#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::core {

enum class HostStatus : int { Unknown = 0, Up = 1, Warning = 2, Down = 3 };

struct Host {
    int64_t id{0};
    std::string name;
    std::string address;
    int pingIntervalSeconds{30};
    int warningThresholdMs{100};
    int criticalThresholdMs{500};
    HostStatus status{HostStatus::Unknown};
    bool enabled{true};
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> lastChecked;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] std::string statusToString() const;
    static HostStatus statusFromString(const std::string& str);

    bool operator==(const Host& other) const = default;
};

} // namespace netpulse::core
