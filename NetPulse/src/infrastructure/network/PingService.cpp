#include "infrastructure/network/PingService.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <random>

#ifdef __linux__
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace netpulse::infra {

namespace {

constexpr uint8_t ICMP_ECHO_REQUEST = 8;
constexpr uint8_t ICMP_ECHO_REPLY = 0;

std::string resolveHostname(const std::string& hostname) {
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0) {
        return hostname;
    }

    char ipStr[INET_ADDRSTRLEN];
    auto* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &addr->sin_addr, ipStr, INET_ADDRSTRLEN);
    freeaddrinfo(result);

    return ipStr;
}

} // namespace

PingService::PingService(AsioContext& context) : context_(context) {
    std::random_device rd;
    identifier_ = static_cast<uint16_t>(rd() & 0xFFFF);
    spdlog::debug("PingService initialized with identifier: {}", identifier_);
}

PingService::~PingService() {
    stopAllMonitoring();
}

uint16_t PingService::calculateChecksum(const uint8_t* data, size_t length) {
    uint32_t sum = 0;

    while (length > 1) {
        sum += (static_cast<uint16_t>(data[0]) << 8) | data[1];
        data += 2;
        length -= 2;
    }

    if (length == 1) {
        sum += static_cast<uint16_t>(data[0]) << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

std::vector<uint8_t> PingService::buildIcmpEchoRequest(uint16_t identifier, uint16_t sequence) {
    std::vector<uint8_t> packet(64, 0);

    // ICMP header
    packet[0] = ICMP_ECHO_REQUEST;  // Type
    packet[1] = 0;                   // Code
    packet[2] = 0;                   // Checksum (high byte)
    packet[3] = 0;                   // Checksum (low byte)
    packet[4] = static_cast<uint8_t>(identifier >> 8);
    packet[5] = static_cast<uint8_t>(identifier & 0xFF);
    packet[6] = static_cast<uint8_t>(sequence >> 8);
    packet[7] = static_cast<uint8_t>(sequence & 0xFF);

    // Timestamp as payload
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::memcpy(&packet[8], &now, sizeof(now));

    // Calculate and set checksum
    uint16_t checksum = calculateChecksum(packet.data(), packet.size());
    packet[2] = static_cast<uint8_t>(checksum >> 8);
    packet[3] = static_cast<uint8_t>(checksum & 0xFF);

    return packet;
}

core::PingResult PingService::performPing(const std::string& address,
                                          std::chrono::milliseconds timeout) {
    core::PingResult result;
    result.timestamp = std::chrono::system_clock::now();
    result.success = false;

#ifdef __linux__
    std::string resolvedAddress = resolveHostname(address);

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        result.errorMessage = "Failed to create raw socket (need CAP_NET_RAW)";
        spdlog::warn("Ping to {} failed: {}", address, result.errorMessage);
        return result;
    }

    // Set receive timeout
    struct timeval tv {};
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest {};
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, resolvedAddress.c_str(), &dest.sin_addr);

    uint16_t seq = sequenceNumber_++;
    auto packet = buildIcmpEchoRequest(identifier_, seq);

    auto sendTime = std::chrono::steady_clock::now();

    ssize_t sent = sendto(sock, packet.data(), packet.size(), 0,
                          reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));

    if (sent < 0) {
        result.errorMessage = "Failed to send ICMP packet";
        close(sock);
        return result;
    }

    std::array<uint8_t, 1024> recvBuffer{};
    struct sockaddr_in from {};
    socklen_t fromLen = sizeof(from);

    ssize_t received = recvfrom(sock, recvBuffer.data(), recvBuffer.size(), 0,
                                reinterpret_cast<struct sockaddr*>(&from), &fromLen);

    auto recvTime = std::chrono::steady_clock::now();
    close(sock);

    if (received < 0) {
        result.errorMessage = "Timeout or receive error";
        return result;
    }

    // Parse IP header and ICMP reply
    if (received >= 28) { // Minimum IP header (20) + ICMP header (8)
        auto* ipHeader = recvBuffer.data();
        size_t ipHeaderLen = static_cast<size_t>((ipHeader[0] & 0x0F) * 4);
        auto* icmpHeader = recvBuffer.data() + ipHeaderLen;

        if (icmpHeader[0] == ICMP_ECHO_REPLY) {
            uint16_t recvId = (static_cast<uint16_t>(icmpHeader[4]) << 8) | icmpHeader[5];
            uint16_t recvSeq = (static_cast<uint16_t>(icmpHeader[6]) << 8) | icmpHeader[7];

            if (recvId == identifier_ && recvSeq == seq) {
                result.success = true;
                result.latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    recvTime - sendTime);
                result.ttl = ipHeader[8]; // TTL field in IP header

                spdlog::debug("Ping to {} successful: {:.2f}ms TTL={}", address,
                              result.latencyMs(), *result.ttl);
            }
        }
    }
#else
    result.errorMessage = "ICMP ping not implemented for this platform";
#endif

    return result;
}

std::future<core::PingResult> PingService::pingAsync(const std::string& address,
                                                     std::chrono::milliseconds timeout) {
    auto promise = std::make_shared<std::promise<core::PingResult>>();
    auto future = promise->get_future();

    context_.post([this, address, timeout, promise]() {
        try {
            auto result = performPing(address, timeout);
            promise->set_value(result);
        } catch (const std::exception& e) {
            core::PingResult result;
            result.timestamp = std::chrono::system_clock::now();
            result.success = false;
            result.errorMessage = e.what();
            promise->set_value(result);
        }
    });

    return future;
}

void PingService::startMonitoring(const core::Host& host, PingCallback callback) {
    std::lock_guard lock(mutex_);

    auto it = monitoredHosts_.find(host.id);
    if (it != monitoredHosts_.end()) {
        it->second->active = false;
        it->second->timer->cancel();
    }

    auto monitored = std::make_shared<MonitoredHost>();
    monitored->host = host;
    monitored->callback = std::move(callback);
    monitored->timer = std::make_shared<asio::steady_timer>(context_.getContext());
    monitored->active = true;

    monitoredHosts_[host.id] = monitored;

    spdlog::info("Started monitoring host: {} ({})", host.name, host.address);
    scheduleNextPing(monitored);
}

void PingService::stopMonitoring(int64_t hostId) {
    std::lock_guard lock(mutex_);

    auto it = monitoredHosts_.find(hostId);
    if (it != monitoredHosts_.end()) {
        it->second->active = false;
        it->second->timer->cancel();
        monitoredHosts_.erase(it);
        spdlog::info("Stopped monitoring host: {}", hostId);
    }
}

void PingService::stopAllMonitoring() {
    std::lock_guard lock(mutex_);

    for (auto& [id, monitored] : monitoredHosts_) {
        monitored->active = false;
        monitored->timer->cancel();
    }
    monitoredHosts_.clear();
    spdlog::info("Stopped all host monitoring");
}

bool PingService::isMonitoring(int64_t hostId) const {
    std::lock_guard lock(mutex_);
    return monitoredHosts_.contains(hostId);
}

void PingService::scheduleNextPing(std::shared_ptr<MonitoredHost> monitored) {
    if (!monitored->active) {
        return;
    }

    monitored->timer->expires_after(std::chrono::seconds(monitored->host.pingIntervalSeconds));
    monitored->timer->async_wait([this, monitored](const asio::error_code& ec) {
        if (ec || !monitored->active) {
            return;
        }

        auto result = performPing(monitored->host.address, std::chrono::milliseconds(5000));
        result.hostId = monitored->host.id;

        if (monitored->callback && monitored->active) {
            monitored->callback(result);
        }

        scheduleNextPing(monitored);
    });
}

} // namespace netpulse::infra
