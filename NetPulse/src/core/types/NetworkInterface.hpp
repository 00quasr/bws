#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace netpulse::core {

struct NetworkInterfaceStats {
    uint64_t bytesReceived{0};
    uint64_t bytesSent{0};
    uint64_t packetsReceived{0};
    uint64_t packetsSent{0};
    uint64_t errorsIn{0};
    uint64_t errorsOut{0};
    uint64_t dropsIn{0};
    uint64_t dropsOut{0};
};

struct NetworkInterface {
    std::string name;
    std::string displayName;
    std::string ipAddress;
    std::string macAddress;
    bool isUp{false};
    bool isLoopback{false};
    NetworkInterfaceStats stats;

    [[nodiscard]] std::string formatBytesReceived() const;
    [[nodiscard]] std::string formatBytesSent() const;

    bool operator==(const NetworkInterface& other) const = default;
};

class NetworkInterfaceEnumerator {
public:
    static std::vector<NetworkInterface> enumerate();
    static NetworkInterfaceStats getStats(const std::string& interfaceName);
};

} // namespace netpulse::core
