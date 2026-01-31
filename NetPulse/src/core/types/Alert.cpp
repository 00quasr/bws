#include "core/types/Alert.hpp"

namespace netpulse::core {

std::string Alert::typeToString() const {
    switch (type) {
    case AlertType::HostDown:
        return "HostDown";
    case AlertType::HighLatency:
        return "HighLatency";
    case AlertType::PacketLoss:
        return "PacketLoss";
    case AlertType::HostRecovered:
        return "HostRecovered";
    case AlertType::ScanComplete:
        return "ScanComplete";
    }
    return "Unknown";
}

std::string Alert::severityToString() const {
    switch (severity) {
    case AlertSeverity::Info:
        return "Info";
    case AlertSeverity::Warning:
        return "Warning";
    case AlertSeverity::Critical:
        return "Critical";
    }
    return "Unknown";
}

AlertType Alert::typeFromString(const std::string& str) {
    if (str == "HostDown")
        return AlertType::HostDown;
    if (str == "HighLatency")
        return AlertType::HighLatency;
    if (str == "PacketLoss")
        return AlertType::PacketLoss;
    if (str == "HostRecovered")
        return AlertType::HostRecovered;
    if (str == "ScanComplete")
        return AlertType::ScanComplete;
    return AlertType::HostDown;
}

AlertSeverity Alert::severityFromString(const std::string& str) {
    if (str == "Info")
        return AlertSeverity::Info;
    if (str == "Warning")
        return AlertSeverity::Warning;
    if (str == "Critical")
        return AlertSeverity::Critical;
    return AlertSeverity::Info;
}

} // namespace netpulse::core
