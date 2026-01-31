#pragma once

#include "core/types/PortScanResult.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace netpulse::core {

enum class PortChangeType : int { NewOpen = 0, NewClosed = 1, StateChanged = 2 };

struct PortChange {
    uint16_t port{0};
    PortChangeType changeType{PortChangeType::StateChanged};
    PortState previousState{PortState::Unknown};
    PortState currentState{PortState::Unknown};
    std::string serviceName;

    [[nodiscard]] std::string changeTypeToString() const;
    static PortChangeType changeTypeFromString(const std::string& str);

    bool operator==(const PortChange& other) const = default;
};

struct PortScanDiff {
    int64_t id{0};
    std::string targetAddress;
    std::chrono::system_clock::time_point previousScanTime;
    std::chrono::system_clock::time_point currentScanTime;
    std::vector<PortChange> changes;
    int totalPortsScanned{0};
    int openPortsBefore{0};
    int openPortsAfter{0};

    [[nodiscard]] bool hasChanges() const { return !changes.empty(); }
    [[nodiscard]] int newOpenPorts() const;
    [[nodiscard]] int newClosedPorts() const;

    bool operator==(const PortScanDiff& other) const = default;
};

struct ScheduledScanConfig {
    int64_t id{0};
    std::string name;
    std::string targetAddress;
    PortRange portRange{PortRange::Common};
    std::vector<uint16_t> customPorts;
    int intervalMinutes{60};
    bool enabled{true};
    bool notifyOnChanges{true};
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> lastRunAt;
    std::optional<std::chrono::system_clock::time_point> nextRunAt;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] PortScanConfig toPortScanConfig() const;

    bool operator==(const ScheduledScanConfig& other) const = default;
};

} // namespace netpulse::core
