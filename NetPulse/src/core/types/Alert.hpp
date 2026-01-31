#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::core {

enum class AlertType : int {
    HostDown = 0,
    HighLatency = 1,
    PacketLoss = 2,
    HostRecovered = 3,
    ScanComplete = 4
};

enum class AlertSeverity : int { Info = 0, Warning = 1, Critical = 2 };

struct Alert {
    int64_t id{0};
    int64_t hostId{0};
    AlertType type{AlertType::HostDown};
    AlertSeverity severity{AlertSeverity::Info};
    std::string title;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    bool acknowledged{false};

    [[nodiscard]] std::string typeToString() const;
    [[nodiscard]] std::string severityToString() const;
    static AlertType typeFromString(const std::string& str);
    static AlertSeverity severityFromString(const std::string& str);

    bool operator==(const Alert& other) const = default;
};

struct AlertThresholds {
    int latencyWarningMs{100};
    int latencyCriticalMs{500};
    double packetLossWarningPercent{5.0};
    double packetLossCriticalPercent{20.0};
    int consecutiveFailuresForDown{3};
};

struct AlertFilter {
    std::optional<AlertSeverity> severity;
    std::optional<AlertType> type;
    std::optional<bool> acknowledged;
    std::string searchText;

    [[nodiscard]] bool isEmpty() const {
        return !severity.has_value() && !type.has_value() && !acknowledged.has_value() &&
               searchText.empty();
    }
};

} // namespace netpulse::core
