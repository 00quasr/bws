#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace netpulse::core {

enum class PortState : int { Unknown = 0, Open = 1, Closed = 2, Filtered = 3 };

struct PortScanResult {
    int64_t id{0};
    std::string targetAddress;
    uint16_t port{0};
    PortState state{PortState::Unknown};
    std::string serviceName;
    std::chrono::system_clock::time_point scanTimestamp;

    [[nodiscard]] std::string stateToString() const;
    static std::string portStateToString(PortState state);
    static PortState stateFromString(const std::string& str);

    bool operator==(const PortScanResult& other) const = default;
};

enum class PortRange { Common, Web, Database, All, Custom };

struct PortScanConfig {
    std::string targetAddress;
    PortRange range{PortRange::Common};
    std::vector<uint16_t> customPorts;
    int maxConcurrency{100};
    std::chrono::milliseconds timeout{1000};

    [[nodiscard]] std::vector<uint16_t> getPortsToScan() const;
};

class ServiceDetector {
public:
    static std::string detectService(uint16_t port);
    static const std::unordered_map<uint16_t, std::string>& getKnownServices();
};

} // namespace netpulse::core
