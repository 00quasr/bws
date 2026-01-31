#include "core/types/Host.hpp"

namespace netpulse::core {

bool Host::isValid() const {
    return !name.empty() && !address.empty() && pingIntervalSeconds > 0 &&
           warningThresholdMs > 0 && criticalThresholdMs > warningThresholdMs;
}

std::string Host::statusToString() const {
    switch (status) {
    case HostStatus::Unknown:
        return "Unknown";
    case HostStatus::Up:
        return "Up";
    case HostStatus::Warning:
        return "Warning";
    case HostStatus::Down:
        return "Down";
    }
    return "Unknown";
}

HostStatus Host::statusFromString(const std::string& str) {
    if (str == "Up")
        return HostStatus::Up;
    if (str == "Warning")
        return HostStatus::Warning;
    if (str == "Down")
        return HostStatus::Down;
    return HostStatus::Unknown;
}

} // namespace netpulse::core
