#include "core/types/ScheduledPortScan.hpp"

namespace netpulse::core {

std::string PortChange::changeTypeToString() const {
    switch (changeType) {
        case PortChangeType::NewOpen:
            return "NewOpen";
        case PortChangeType::NewClosed:
            return "NewClosed";
        case PortChangeType::StateChanged:
            return "StateChanged";
    }
    return "Unknown";
}

PortChangeType PortChange::changeTypeFromString(const std::string& str) {
    if (str == "NewOpen") return PortChangeType::NewOpen;
    if (str == "NewClosed") return PortChangeType::NewClosed;
    return PortChangeType::StateChanged;
}

int PortScanDiff::newOpenPorts() const {
    int count = 0;
    for (const auto& change : changes) {
        if (change.changeType == PortChangeType::NewOpen) {
            ++count;
        }
    }
    return count;
}

int PortScanDiff::newClosedPorts() const {
    int count = 0;
    for (const auto& change : changes) {
        if (change.changeType == PortChangeType::NewClosed) {
            ++count;
        }
    }
    return count;
}

bool ScheduledScanConfig::isValid() const {
    if (name.empty()) return false;
    if (targetAddress.empty()) return false;
    if (intervalMinutes < 1) return false;
    if (portRange == PortRange::Custom && customPorts.empty()) return false;
    return true;
}

PortScanConfig ScheduledScanConfig::toPortScanConfig() const {
    PortScanConfig config;
    config.targetAddress = targetAddress;
    config.range = portRange;
    config.customPorts = customPorts;
    return config;
}

} // namespace netpulse::core
