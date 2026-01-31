#include "core/types/NetworkInterface.hpp"

#include <fstream>
#include <sstream>

#ifdef __linux__
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace netpulse::core {

namespace {

std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    auto value = static_cast<double>(bytes);

    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed << value << " " << units[unitIndex];
    return oss.str();
}

} // namespace

std::string NetworkInterface::formatBytesReceived() const {
    return formatBytes(stats.bytesReceived);
}

std::string NetworkInterface::formatBytesSent() const {
    return formatBytes(stats.bytesSent);
}

std::vector<NetworkInterface> NetworkInterfaceEnumerator::enumerate() {
    std::vector<NetworkInterface> interfaces;

#ifdef __linux__
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        return interfaces;
    }

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        if (ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        NetworkInterface iface;
        iface.name = ifa->ifa_name;
        iface.displayName = ifa->ifa_name;
        iface.isUp = (ifa->ifa_flags & IFF_UP) != 0;
        iface.isLoopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;

        char ipStr[INET_ADDRSTRLEN];
        auto* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
        inet_ntop(AF_INET, &addr->sin_addr, ipStr, INET_ADDRSTRLEN);
        iface.ipAddress = ipStr;

        iface.stats = getStats(iface.name);
        interfaces.push_back(std::move(iface));
    }

    freeifaddrs(ifaddr);
#endif

    return interfaces;
}

NetworkInterfaceStats NetworkInterfaceEnumerator::getStats(const std::string& interfaceName) {
    NetworkInterfaceStats stats;

#ifdef __linux__
    std::string path = "/sys/class/net/" + interfaceName + "/statistics/";

    auto readStat = [&path](const std::string& name) -> uint64_t {
        std::ifstream file(path + name);
        uint64_t value = 0;
        if (file) {
            file >> value;
        }
        return value;
    };

    stats.bytesReceived = readStat("rx_bytes");
    stats.bytesSent = readStat("tx_bytes");
    stats.packetsReceived = readStat("rx_packets");
    stats.packetsSent = readStat("tx_packets");
    stats.errorsIn = readStat("rx_errors");
    stats.errorsOut = readStat("tx_errors");
    stats.dropsIn = readStat("rx_dropped");
    stats.dropsOut = readStat("tx_dropped");
#endif

    return stats;
}

} // namespace netpulse::core
