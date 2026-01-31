#include "infrastructure/network/SnmpService.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace netpulse::infra {

namespace {
// ASN.1/BER tag types
constexpr uint8_t TAG_INTEGER = 0x02;
constexpr uint8_t TAG_OCTET_STRING = 0x04;
constexpr uint8_t TAG_NULL = 0x05;
constexpr uint8_t TAG_OID = 0x06;
constexpr uint8_t TAG_SEQUENCE = 0x30;
constexpr uint8_t TAG_IP_ADDRESS = 0x40;
constexpr uint8_t TAG_COUNTER32 = 0x41;
constexpr uint8_t TAG_GAUGE32 = 0x42;
constexpr uint8_t TAG_TIMETICKS = 0x43;
constexpr uint8_t TAG_COUNTER64 = 0x46;
constexpr uint8_t TAG_NO_SUCH_OBJECT = 0x80;
constexpr uint8_t TAG_NO_SUCH_INSTANCE = 0x81;
constexpr uint8_t TAG_END_OF_MIB_VIEW = 0x82;

// SNMP versions
constexpr int32_t SNMP_VERSION_1 = 0;
constexpr int32_t SNMP_VERSION_2C = 1;
constexpr int32_t SNMP_VERSION_3 = 3;

// SNMP error statuses
constexpr int SNMP_ERR_NO_ERROR = 0;
constexpr int SNMP_ERR_TOO_BIG = 1;
constexpr int SNMP_ERR_NO_SUCH_NAME = 2;
constexpr int SNMP_ERR_BAD_VALUE = 3;
constexpr int SNMP_ERR_READ_ONLY = 4;
constexpr int SNMP_ERR_GEN_ERR = 5;

std::string snmpErrorToString(int errorStatus) {
    switch (errorStatus) {
        case SNMP_ERR_NO_ERROR: return "No error";
        case SNMP_ERR_TOO_BIG: return "Response too big";
        case SNMP_ERR_NO_SUCH_NAME: return "No such name";
        case SNMP_ERR_BAD_VALUE: return "Bad value";
        case SNMP_ERR_READ_ONLY: return "Read only";
        case SNMP_ERR_GEN_ERR: return "General error";
        default: return "Unknown error";
    }
}

core::SnmpDataType tagToDataType(uint8_t tag) {
    switch (tag) {
        case TAG_INTEGER: return core::SnmpDataType::Integer;
        case TAG_OCTET_STRING: return core::SnmpDataType::OctetString;
        case TAG_OID: return core::SnmpDataType::ObjectIdentifier;
        case TAG_IP_ADDRESS: return core::SnmpDataType::IpAddress;
        case TAG_COUNTER32: return core::SnmpDataType::Counter32;
        case TAG_GAUGE32: return core::SnmpDataType::Gauge32;
        case TAG_TIMETICKS: return core::SnmpDataType::TimeTicks;
        case TAG_COUNTER64: return core::SnmpDataType::Counter64;
        case TAG_NULL: return core::SnmpDataType::Null;
        case TAG_NO_SUCH_OBJECT: return core::SnmpDataType::NoSuchObject;
        case TAG_NO_SUCH_INSTANCE: return core::SnmpDataType::NoSuchInstance;
        case TAG_END_OF_MIB_VIEW: return core::SnmpDataType::EndOfMibView;
        default: return core::SnmpDataType::Unknown;
    }
}

} // anonymous namespace

SnmpService::SnmpService(AsioContext& context)
    : context_(context) {
    // Initialize request ID with random value for security
    std::random_device rd;
    requestIdCounter_ = static_cast<int32_t>(rd() & 0x7FFFFFFF);
    spdlog::debug("SnmpService initialized");
}

SnmpService::~SnmpService() {
    stopAllMonitoring();
}

std::future<core::SnmpResult> SnmpService::getAsync(
    const std::string& address,
    const std::vector<std::string>& oids,
    const core::SnmpDeviceConfig& config) {

    auto promise = std::make_shared<std::promise<core::SnmpResult>>();
    auto future = promise->get_future();

    context_.post([this, promise, address, oids, config]() {
        try {
            auto result = performSnmpGet(address, oids, config, PduType::GetRequest);
            promise->set_value(result);
        } catch (const std::exception& e) {
            core::SnmpResult result;
            result.timestamp = std::chrono::system_clock::now();
            result.success = false;
            result.errorMessage = e.what();
            promise->set_value(result);
        }
    });

    return future;
}

std::future<core::SnmpResult> SnmpService::getNextAsync(
    const std::string& address,
    const std::vector<std::string>& oids,
    const core::SnmpDeviceConfig& config) {

    auto promise = std::make_shared<std::promise<core::SnmpResult>>();
    auto future = promise->get_future();

    context_.post([this, promise, address, oids, config]() {
        try {
            auto result = performSnmpGet(address, oids, config, PduType::GetNextRequest);
            promise->set_value(result);
        } catch (const std::exception& e) {
            core::SnmpResult result;
            result.timestamp = std::chrono::system_clock::now();
            result.success = false;
            result.errorMessage = e.what();
            promise->set_value(result);
        }
    });

    return future;
}

std::future<std::vector<core::SnmpVarBind>> SnmpService::walkAsync(
    const std::string& address,
    const std::string& rootOid,
    const core::SnmpDeviceConfig& config) {

    auto promise = std::make_shared<std::promise<std::vector<core::SnmpVarBind>>>();
    auto future = promise->get_future();

    context_.post([this, promise, address, rootOid, config]() {
        std::vector<core::SnmpVarBind> results;
        std::string currentOid = rootOid;

        try {
            constexpr int maxIterations = 1000;  // Prevent infinite loops
            int iterations = 0;

            while (iterations++ < maxIterations) {
                auto result = performSnmpGet(address, {currentOid}, config, PduType::GetNextRequest);

                if (!result.success || result.varbinds.empty()) {
                    break;
                }

                const auto& vb = result.varbinds[0];

                // Check if we've gone past the subtree
                if (!isOidPrefix(rootOid, vb.oid)) {
                    break;
                }

                // Check for end-of-mib-view
                if (vb.type == core::SnmpDataType::EndOfMibView ||
                    vb.type == core::SnmpDataType::NoSuchObject ||
                    vb.type == core::SnmpDataType::NoSuchInstance) {
                    break;
                }

                results.push_back(vb);
                currentOid = vb.oid;
            }

            promise->set_value(results);
        } catch (const std::exception& e) {
            spdlog::error("SNMP walk error: {}", e.what());
            promise->set_value(results);  // Return partial results
        }
    });

    return future;
}

void SnmpService::startMonitoring(const core::Host& host,
                                   const core::SnmpDeviceConfig& config,
                                   SnmpCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Stop existing monitoring for this host
    auto it = monitoredDevices_.find(host.id);
    if (it != monitoredDevices_.end()) {
        it->second->active = false;
        if (it->second->timer) {
            it->second->timer->cancel();
        }
        monitoredDevices_.erase(it);
    }

    // Create new monitored device
    auto device = std::make_shared<MonitoredDevice>();
    device->host = host;
    device->config = config;
    device->callback = std::move(callback);
    device->timer = std::make_shared<asio::steady_timer>(context_.getContext());
    device->active = true;
    device->statistics.hostId = host.id;

    monitoredDevices_[host.id] = device;

    spdlog::info("Started SNMP monitoring for host {} ({})",
                 host.name, host.address);

    // Schedule first poll
    scheduleNextPoll(device);
}

void SnmpService::stopMonitoring(int64_t hostId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = monitoredDevices_.find(hostId);
    if (it != monitoredDevices_.end()) {
        it->second->active = false;
        if (it->second->timer) {
            it->second->timer->cancel();
        }
        spdlog::info("Stopped SNMP monitoring for host ID {}", hostId);
        monitoredDevices_.erase(it);
    }
}

void SnmpService::stopAllMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [id, device] : monitoredDevices_) {
        device->active = false;
        if (device->timer) {
            device->timer->cancel();
        }
    }
    monitoredDevices_.clear();
    spdlog::info("Stopped all SNMP monitoring");
}

bool SnmpService::isMonitoring(int64_t hostId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return monitoredDevices_.find(hostId) != monitoredDevices_.end();
}

void SnmpService::updateConfig(int64_t hostId, const core::SnmpDeviceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = monitoredDevices_.find(hostId);
    if (it != monitoredDevices_.end()) {
        it->second->config = config;
        spdlog::debug("Updated SNMP config for host ID {}", hostId);
    }
}

core::SnmpStatistics SnmpService::getStatistics(int64_t hostId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = monitoredDevices_.find(hostId);
    if (it != monitoredDevices_.end()) {
        return it->second->statistics;
    }
    return core::SnmpStatistics{};
}

void SnmpService::scheduleNextPoll(std::shared_ptr<MonitoredDevice> device) {
    if (!device->active) {
        return;
    }

    auto interval = std::chrono::seconds(device->config.pollIntervalSeconds);
    device->timer->expires_after(interval);

    device->timer->async_wait([this, device](const asio::error_code& ec) {
        if (ec || !device->active) {
            return;
        }

        // Perform SNMP poll
        auto result = performSnmpGet(device->host.address,
                                      device->config.oids,
                                      device->config,
                                      PduType::GetRequest);
        result.hostId = device->host.id;

        // Update statistics
        device->statistics.totalPolls++;
        if (result.success) {
            device->statistics.successfulPolls++;

            // Update response time stats
            if (device->statistics.minResponseTime.count() == 0 ||
                result.responseTime < device->statistics.minResponseTime) {
                device->statistics.minResponseTime = result.responseTime;
            }
            if (result.responseTime > device->statistics.maxResponseTime) {
                device->statistics.maxResponseTime = result.responseTime;
            }

            // Calculate running average
            auto total = device->statistics.avgResponseTime.count() *
                         (device->statistics.successfulPolls - 1);
            device->statistics.avgResponseTime = std::chrono::microseconds(
                (total + result.responseTime.count()) / device->statistics.successfulPolls);

            // Store last values
            for (const auto& vb : result.varbinds) {
                device->statistics.lastValues[vb.oid] = vb.value;
            }
        }
        device->statistics.successRate = device->statistics.calculateSuccessRate();

        // Invoke callback
        if (device->callback) {
            device->callback(result);
        }

        // Schedule next poll
        scheduleNextPoll(device);
    });
}

core::SnmpResult SnmpService::performSnmpGet(
    const std::string& address,
    const std::vector<std::string>& oids,
    const core::SnmpDeviceConfig& config,
    PduType pduType) {

    core::SnmpResult result;
    result.timestamp = std::chrono::system_clock::now();
    result.version = config.version;

    auto startTime = std::chrono::steady_clock::now();

    try {
        // Resolve address
        asio::io_context tempContext;
        asio::ip::udp::resolver resolver(tempContext);
        auto endpoints = resolver.resolve(asio::ip::udp::v4(),
                                           address,
                                           std::to_string(config.port));

        if (endpoints.empty()) {
            result.success = false;
            result.errorMessage = "Failed to resolve address: " + address;
            return result;
        }

        auto endpoint = *endpoints.begin();

        // Create UDP socket
        asio::ip::udp::socket socket(tempContext, asio::ip::udp::v4());

        // Build SNMP request
        int32_t requestId = requestIdCounter_++;
        std::vector<uint8_t> packet;

        if (config.version == core::SnmpVersion::V3) {
            packet = buildSnmpV3Packet(oids, config, pduType, requestId);
        } else {
            packet = buildSnmpPacket(oids, config, pduType, requestId);
        }

        // Send request
        socket.send_to(asio::buffer(packet), endpoint.endpoint());

        // Set up receive with timeout
        std::vector<uint8_t> recvBuffer(65535);
        asio::ip::udp::endpoint senderEndpoint;

        // Configure socket timeout
        struct timeval tv;
        tv.tv_sec = config.timeoutMs / 1000;
        tv.tv_usec = (config.timeoutMs % 1000) * 1000;
#ifdef _WIN32
        DWORD timeout = config.timeoutMs;
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv));
#endif

        // Receive response
        asio::error_code ec;
        size_t bytesReceived = socket.receive_from(
            asio::buffer(recvBuffer), senderEndpoint, 0, ec);

        auto endTime = std::chrono::steady_clock::now();
        result.responseTime = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);

        if (ec) {
            result.success = false;
            if (ec == asio::error::timed_out ||
                ec == asio::error::would_block) {
                result.errorMessage = "Request timed out";
            } else {
                result.errorMessage = "Receive error: " + ec.message();
            }
            return result;
        }

        // Parse response
        recvBuffer.resize(bytesReceived);
        result = parseSnmpResponse(recvBuffer, config);
        result.responseTime = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);
        result.version = config.version;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = std::string("SNMP error: ") + e.what();
    }

    return result;
}

std::vector<uint8_t> SnmpService::buildSnmpPacket(
    const std::vector<std::string>& oids,
    const core::SnmpDeviceConfig& config,
    PduType pduType,
    int32_t requestId) {

    std::vector<uint8_t> packet;

    // Build varbind list
    std::vector<uint8_t> varbindList;
    for (const auto& oid : oids) {
        std::vector<uint8_t> varbind;
        auto encodedOid = encodeOid(oid);
        auto encodedNull = encodeNull();
        varbind.insert(varbind.end(), encodedOid.begin(), encodedOid.end());
        varbind.insert(varbind.end(), encodedNull.begin(), encodedNull.end());

        auto varbindSeq = encodeSequence(varbind);
        varbindList.insert(varbindList.end(), varbindSeq.begin(), varbindSeq.end());
    }
    auto varbindListSeq = encodeSequence(varbindList);

    // Build PDU
    std::vector<uint8_t> pduContent;
    auto encodedRequestId = encodeInteger(requestId);
    auto encodedErrorStatus = encodeInteger(0);  // No error
    auto encodedErrorIndex = encodeInteger(0);   // No error

    pduContent.insert(pduContent.end(), encodedRequestId.begin(), encodedRequestId.end());
    pduContent.insert(pduContent.end(), encodedErrorStatus.begin(), encodedErrorStatus.end());
    pduContent.insert(pduContent.end(), encodedErrorIndex.begin(), encodedErrorIndex.end());
    pduContent.insert(pduContent.end(), varbindListSeq.begin(), varbindListSeq.end());

    // Wrap in PDU type
    std::vector<uint8_t> pdu;
    pdu.push_back(static_cast<uint8_t>(pduType));
    auto pduLen = encodeLength(pduContent.size());
    pdu.insert(pdu.end(), pduLen.begin(), pduLen.end());
    pdu.insert(pdu.end(), pduContent.begin(), pduContent.end());

    // Get community string
    std::string community = "public";
    if (auto* v2cCreds = std::get_if<core::SnmpV2cCredentials>(&config.credentials)) {
        community = v2cCreds->community;
    }

    // Build message
    std::vector<uint8_t> msgContent;
    int32_t version = (config.version == core::SnmpVersion::V1) ?
                       SNMP_VERSION_1 : SNMP_VERSION_2C;
    auto encodedVersion = encodeInteger(version);
    auto encodedCommunity = encodeOctetString(community);

    msgContent.insert(msgContent.end(), encodedVersion.begin(), encodedVersion.end());
    msgContent.insert(msgContent.end(), encodedCommunity.begin(), encodedCommunity.end());
    msgContent.insert(msgContent.end(), pdu.begin(), pdu.end());

    packet = encodeSequence(msgContent);

    return packet;
}

std::vector<uint8_t> SnmpService::buildSnmpV3Packet(
    const std::vector<std::string>& oids,
    const core::SnmpDeviceConfig& config,
    PduType pduType,
    int32_t requestId) {

    // SNMPv3 packet structure:
    // SEQUENCE {
    //   version (INTEGER 3),
    //   msgGlobalData (SEQUENCE),
    //   msgSecurityParameters (OCTET STRING),
    //   msgData (SEQUENCE or ScopedPDU)
    // }

    auto* v3Creds = std::get_if<core::SnmpV3Credentials>(&config.credentials);
    if (!v3Creds) {
        // Fall back to v2c if no v3 credentials
        return buildSnmpPacket(oids, config, pduType, requestId);
    }

    std::vector<uint8_t> packet;

    // Build varbind list (same as v2c)
    std::vector<uint8_t> varbindList;
    for (const auto& oid : oids) {
        std::vector<uint8_t> varbind;
        auto encodedOid = encodeOid(oid);
        auto encodedNull = encodeNull();
        varbind.insert(varbind.end(), encodedOid.begin(), encodedOid.end());
        varbind.insert(varbind.end(), encodedNull.begin(), encodedNull.end());

        auto varbindSeq = encodeSequence(varbind);
        varbindList.insert(varbindList.end(), varbindSeq.begin(), varbindSeq.end());
    }
    auto varbindListSeq = encodeSequence(varbindList);

    // Build PDU
    std::vector<uint8_t> pduContent;
    auto encodedRequestId = encodeInteger(requestId);
    auto encodedErrorStatus = encodeInteger(0);
    auto encodedErrorIndex = encodeInteger(0);

    pduContent.insert(pduContent.end(), encodedRequestId.begin(), encodedRequestId.end());
    pduContent.insert(pduContent.end(), encodedErrorStatus.begin(), encodedErrorStatus.end());
    pduContent.insert(pduContent.end(), encodedErrorIndex.begin(), encodedErrorIndex.end());
    pduContent.insert(pduContent.end(), varbindListSeq.begin(), varbindListSeq.end());

    // Wrap PDU
    std::vector<uint8_t> pdu;
    pdu.push_back(static_cast<uint8_t>(pduType));
    auto pduLen = encodeLength(pduContent.size());
    pdu.insert(pdu.end(), pduLen.begin(), pduLen.end());
    pdu.insert(pdu.end(), pduContent.begin(), pduContent.end());

    // Build ScopedPDU
    std::vector<uint8_t> scopedPduContent;
    auto contextEngineId = encodeOctetString(v3Creds->contextEngineId);
    auto contextName = encodeOctetString(v3Creds->contextName);
    scopedPduContent.insert(scopedPduContent.end(), contextEngineId.begin(), contextEngineId.end());
    scopedPduContent.insert(scopedPduContent.end(), contextName.begin(), contextName.end());
    scopedPduContent.insert(scopedPduContent.end(), pdu.begin(), pdu.end());
    auto scopedPdu = encodeSequence(scopedPduContent);

    // Build msgSecurityParameters (USM)
    std::vector<uint8_t> usmContent;
    auto authEngineId = encodeOctetString("");  // Will be discovered
    auto authEngineBoots = encodeInteger(0);
    auto authEngineTime = encodeInteger(0);
    auto userName = encodeOctetString(v3Creds->username);
    auto authParams = encodeOctetString("");  // No auth for noAuthNoPriv
    auto privParams = encodeOctetString("");  // No priv for noAuthNoPriv

    usmContent.insert(usmContent.end(), authEngineId.begin(), authEngineId.end());
    usmContent.insert(usmContent.end(), authEngineBoots.begin(), authEngineBoots.end());
    usmContent.insert(usmContent.end(), authEngineTime.begin(), authEngineTime.end());
    usmContent.insert(usmContent.end(), userName.begin(), userName.end());
    usmContent.insert(usmContent.end(), authParams.begin(), authParams.end());
    usmContent.insert(usmContent.end(), privParams.begin(), privParams.end());
    auto usmSeq = encodeSequence(usmContent);
    auto msgSecurityParams = encodeOctetString(
        std::string(reinterpret_cast<const char*>(usmSeq.data()), usmSeq.size()));

    // Build msgGlobalData
    std::vector<uint8_t> globalDataContent;
    auto msgId = encodeInteger(requestId);
    auto msgMaxSize = encodeInteger(65535);

    // msgFlags: reportable, auth, priv bits
    uint8_t flags = 0x04;  // reportable
    if (v3Creds->securityLevel >= core::SnmpSecurityLevel::AuthNoPriv) {
        flags |= 0x01;  // auth
    }
    if (v3Creds->securityLevel == core::SnmpSecurityLevel::AuthPriv) {
        flags |= 0x02;  // priv
    }
    auto msgFlags = encodeOctetString(std::string(1, static_cast<char>(flags)));
    auto msgSecurityModel = encodeInteger(3);  // USM

    globalDataContent.insert(globalDataContent.end(), msgId.begin(), msgId.end());
    globalDataContent.insert(globalDataContent.end(), msgMaxSize.begin(), msgMaxSize.end());
    globalDataContent.insert(globalDataContent.end(), msgFlags.begin(), msgFlags.end());
    globalDataContent.insert(globalDataContent.end(), msgSecurityModel.begin(), msgSecurityModel.end());
    auto globalData = encodeSequence(globalDataContent);

    // Build complete message
    std::vector<uint8_t> msgContent;
    auto version = encodeInteger(SNMP_VERSION_3);
    msgContent.insert(msgContent.end(), version.begin(), version.end());
    msgContent.insert(msgContent.end(), globalData.begin(), globalData.end());
    msgContent.insert(msgContent.end(), msgSecurityParams.begin(), msgSecurityParams.end());
    msgContent.insert(msgContent.end(), scopedPdu.begin(), scopedPdu.end());

    packet = encodeSequence(msgContent);

    return packet;
}

core::SnmpResult SnmpService::parseSnmpResponse(
    const std::vector<uint8_t>& response,
    const core::SnmpDeviceConfig& config) {

    core::SnmpResult result;
    result.timestamp = std::chrono::system_clock::now();
    result.version = config.version;

    if (response.size() < 10) {
        result.success = false;
        result.errorMessage = "Response too short";
        return result;
    }

    try {
        size_t offset = 0;
        const uint8_t* data = response.data();

        // Outer SEQUENCE
        if (data[offset++] != TAG_SEQUENCE) {
            throw std::runtime_error("Expected SEQUENCE");
        }
        decodeLength(data, offset);

        // Version
        if (data[offset++] != TAG_INTEGER) {
            throw std::runtime_error("Expected INTEGER for version");
        }
        size_t versionLen = decodeLength(data, offset);
        int32_t version = decodeInteger(data + offset, versionLen);
        offset += versionLen;

        if (version == SNMP_VERSION_3) {
            // Skip v3 header parsing for now - just extract the PDU
            // In a full implementation, we'd verify authentication here

            // Skip msgGlobalData
            if (data[offset++] != TAG_SEQUENCE) {
                throw std::runtime_error("Expected SEQUENCE for msgGlobalData");
            }
            size_t globalDataLen = decodeLength(data, offset);
            offset += globalDataLen;

            // Skip msgSecurityParameters
            if (data[offset++] != TAG_OCTET_STRING) {
                throw std::runtime_error("Expected OCTET STRING for msgSecurityParameters");
            }
            size_t secParamsLen = decodeLength(data, offset);
            offset += secParamsLen;

            // msgData (ScopedPDU)
            if (data[offset++] != TAG_SEQUENCE) {
                throw std::runtime_error("Expected SEQUENCE for ScopedPDU");
            }
            decodeLength(data, offset);

            // contextEngineID
            if (data[offset++] != TAG_OCTET_STRING) {
                throw std::runtime_error("Expected OCTET STRING for contextEngineID");
            }
            size_t engineIdLen = decodeLength(data, offset);
            offset += engineIdLen;

            // contextName
            if (data[offset++] != TAG_OCTET_STRING) {
                throw std::runtime_error("Expected OCTET STRING for contextName");
            }
            size_t contextNameLen = decodeLength(data, offset);
            offset += contextNameLen;

        } else {
            // v1/v2c: skip community string
            if (data[offset++] != TAG_OCTET_STRING) {
                throw std::runtime_error("Expected OCTET STRING for community");
            }
            size_t communityLen = decodeLength(data, offset);
            offset += communityLen;
        }

        // PDU (GetResponse = 0xA2)
        uint8_t pduType = data[offset++];
        if (pduType != 0xA2) {
            throw std::runtime_error("Expected GetResponse PDU");
        }
        decodeLength(data, offset);

        // Request ID
        if (data[offset++] != TAG_INTEGER) {
            throw std::runtime_error("Expected INTEGER for request-id");
        }
        size_t reqIdLen = decodeLength(data, offset);
        offset += reqIdLen;

        // Error status
        if (data[offset++] != TAG_INTEGER) {
            throw std::runtime_error("Expected INTEGER for error-status");
        }
        size_t errorStatusLen = decodeLength(data, offset);
        result.errorStatus = decodeInteger(data + offset, errorStatusLen);
        offset += errorStatusLen;

        // Error index
        if (data[offset++] != TAG_INTEGER) {
            throw std::runtime_error("Expected INTEGER for error-index");
        }
        size_t errorIndexLen = decodeLength(data, offset);
        result.errorIndex = decodeInteger(data + offset, errorIndexLen);
        offset += errorIndexLen;

        if (result.errorStatus != SNMP_ERR_NO_ERROR) {
            result.success = false;
            result.errorMessage = snmpErrorToString(result.errorStatus);
            return result;
        }

        // VarBind list
        if (data[offset++] != TAG_SEQUENCE) {
            throw std::runtime_error("Expected SEQUENCE for varbind-list");
        }
        size_t varbindListLen = decodeLength(data, offset);
        size_t varbindListEnd = offset + varbindListLen;

        // Parse each varbind
        while (offset < varbindListEnd && offset < response.size()) {
            if (data[offset++] != TAG_SEQUENCE) {
                throw std::runtime_error("Expected SEQUENCE for varbind");
            }
            decodeLength(data, offset);

            // OID
            if (data[offset++] != TAG_OID) {
                throw std::runtime_error("Expected OID");
            }
            size_t oidLen = decodeLength(data, offset);
            std::string oid = decodeOid(data + offset, oidLen);
            offset += oidLen;

            // Value
            core::SnmpVarBind varbind;
            varbind.oid = oid;
            uint8_t valueTag = data[offset++];
            size_t valueLen = decodeLength(data, offset);

            varbind.type = tagToDataType(valueTag);

            switch (valueTag) {
                case TAG_INTEGER:
                    varbind.intValue = decodeInteger(data + offset, valueLen);
                    varbind.value = std::to_string(*varbind.intValue);
                    break;
                case TAG_OCTET_STRING:
                    varbind.value = decodeOctetString(data + offset, valueLen);
                    break;
                case TAG_OID:
                    varbind.value = decodeOid(data + offset, valueLen);
                    break;
                case TAG_IP_ADDRESS:
                    if (valueLen == 4) {
                        varbind.value = std::to_string(data[offset]) + "." +
                                        std::to_string(data[offset + 1]) + "." +
                                        std::to_string(data[offset + 2]) + "." +
                                        std::to_string(data[offset + 3]);
                    }
                    break;
                case TAG_COUNTER32:
                case TAG_GAUGE32:
                case TAG_TIMETICKS: {
                    uint64_t val = 0;
                    for (size_t i = 0; i < valueLen; ++i) {
                        val = (val << 8) | data[offset + i];
                    }
                    varbind.counterValue = val;
                    varbind.value = std::to_string(val);
                    break;
                }
                case TAG_COUNTER64:
                    varbind.counterValue = decodeCounter64(data + offset, valueLen);
                    varbind.value = std::to_string(*varbind.counterValue);
                    break;
                case TAG_NULL:
                case TAG_NO_SUCH_OBJECT:
                case TAG_NO_SUCH_INSTANCE:
                case TAG_END_OF_MIB_VIEW:
                    varbind.value = "";
                    break;
                default:
                    // Store as hex string for unknown types
                    std::ostringstream oss;
                    oss << std::hex;
                    for (size_t i = 0; i < valueLen; ++i) {
                        oss << std::setw(2) << std::setfill('0')
                            << static_cast<int>(data[offset + i]);
                    }
                    varbind.value = oss.str();
                    break;
            }

            offset += valueLen;
            result.varbinds.push_back(varbind);
        }

        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = std::string("Parse error: ") + e.what();
    }

    return result;
}

// BER encoding helpers

std::vector<uint8_t> SnmpService::encodeLength(size_t length) {
    std::vector<uint8_t> encoded;

    if (length < 128) {
        encoded.push_back(static_cast<uint8_t>(length));
    } else if (length < 256) {
        encoded.push_back(0x81);
        encoded.push_back(static_cast<uint8_t>(length));
    } else if (length < 65536) {
        encoded.push_back(0x82);
        encoded.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
        encoded.push_back(static_cast<uint8_t>(length & 0xFF));
    } else {
        encoded.push_back(0x83);
        encoded.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
        encoded.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
        encoded.push_back(static_cast<uint8_t>(length & 0xFF));
    }

    return encoded;
}

std::vector<uint8_t> SnmpService::encodeInteger(int32_t value) {
    std::vector<uint8_t> encoded;
    encoded.push_back(TAG_INTEGER);

    std::vector<uint8_t> bytes;

    if (value == 0) {
        bytes.push_back(0);
    } else if (value > 0) {
        while (value > 0) {
            bytes.insert(bytes.begin(), static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
        // Add leading zero if high bit is set
        if (bytes[0] & 0x80) {
            bytes.insert(bytes.begin(), 0);
        }
    } else {
        // Negative numbers
        while (value < -1 || (value == -1 && !(bytes.empty() || (bytes[0] & 0x80)))) {
            bytes.insert(bytes.begin(), static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }

    auto len = encodeLength(bytes.size());
    encoded.insert(encoded.end(), len.begin(), len.end());
    encoded.insert(encoded.end(), bytes.begin(), bytes.end());

    return encoded;
}

std::vector<uint8_t> SnmpService::encodeOctetString(const std::string& str) {
    std::vector<uint8_t> encoded;
    encoded.push_back(TAG_OCTET_STRING);
    auto len = encodeLength(str.size());
    encoded.insert(encoded.end(), len.begin(), len.end());
    encoded.insert(encoded.end(), str.begin(), str.end());
    return encoded;
}

std::vector<uint8_t> SnmpService::encodeOid(const std::string& oid) {
    std::vector<uint8_t> encoded;
    encoded.push_back(TAG_OID);

    auto components = parseOidString(oid);
    std::vector<uint8_t> oidBytes;

    if (components.size() >= 2) {
        // First two components are encoded as (first * 40 + second)
        oidBytes.push_back(static_cast<uint8_t>(components[0] * 40 + components[1]));

        // Remaining components use base-128 encoding
        for (size_t i = 2; i < components.size(); ++i) {
            uint32_t val = components[i];
            if (val == 0) {
                oidBytes.push_back(0);
            } else {
                std::vector<uint8_t> subId;
                while (val > 0) {
                    subId.insert(subId.begin(), static_cast<uint8_t>(val & 0x7F));
                    val >>= 7;
                }
                // Set high bit on all but last byte
                for (size_t j = 0; j < subId.size() - 1; ++j) {
                    subId[j] |= 0x80;
                }
                oidBytes.insert(oidBytes.end(), subId.begin(), subId.end());
            }
        }
    }

    auto len = encodeLength(oidBytes.size());
    encoded.insert(encoded.end(), len.begin(), len.end());
    encoded.insert(encoded.end(), oidBytes.begin(), oidBytes.end());

    return encoded;
}

std::vector<uint8_t> SnmpService::encodeNull() {
    return {TAG_NULL, 0x00};
}

std::vector<uint8_t> SnmpService::encodeSequence(const std::vector<uint8_t>& content) {
    std::vector<uint8_t> encoded;
    encoded.push_back(TAG_SEQUENCE);
    auto len = encodeLength(content.size());
    encoded.insert(encoded.end(), len.begin(), len.end());
    encoded.insert(encoded.end(), content.begin(), content.end());
    return encoded;
}

// BER decoding helpers

size_t SnmpService::decodeLength(const uint8_t* data, size_t& offset) {
    uint8_t first = data[offset++];

    if ((first & 0x80) == 0) {
        return first;
    }

    size_t numBytes = first & 0x7F;
    size_t length = 0;

    for (size_t i = 0; i < numBytes; ++i) {
        length = (length << 8) | data[offset++];
    }

    return length;
}

int32_t SnmpService::decodeInteger(const uint8_t* data, size_t length) {
    if (length == 0) return 0;

    int32_t value = 0;
    bool negative = (data[0] & 0x80) != 0;

    for (size_t i = 0; i < length; ++i) {
        value = (value << 8) | data[i];
    }

    // Sign extend if negative
    if (negative && length < 4) {
        value |= static_cast<int32_t>(0xFFFFFFFF << (length * 8));
    }

    return value;
}

std::string SnmpService::decodeOctetString(const uint8_t* data, size_t length) {
    return std::string(reinterpret_cast<const char*>(data), length);
}

std::string SnmpService::decodeOid(const uint8_t* data, size_t length) {
    if (length == 0) return "";

    std::vector<uint32_t> components;

    // First byte encodes first two components
    components.push_back(data[0] / 40);
    components.push_back(data[0] % 40);

    // Remaining bytes use base-128 encoding
    uint32_t value = 0;
    for (size_t i = 1; i < length; ++i) {
        value = (value << 7) | (data[i] & 0x7F);
        if ((data[i] & 0x80) == 0) {
            components.push_back(value);
            value = 0;
        }
    }

    return oidVectorToString(components);
}

uint64_t SnmpService::decodeCounter64(const uint8_t* data, size_t length) {
    uint64_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        value = (value << 8) | data[i];
    }
    return value;
}

std::vector<uint32_t> SnmpService::parseOidString(const std::string& oid) {
    std::vector<uint32_t> components;
    std::istringstream iss(oid);
    std::string token;

    while (std::getline(iss, token, '.')) {
        if (!token.empty()) {
            components.push_back(static_cast<uint32_t>(std::stoul(token)));
        }
    }

    return components;
}

std::string SnmpService::oidVectorToString(const std::vector<uint32_t>& oid) {
    std::ostringstream oss;
    for (size_t i = 0; i < oid.size(); ++i) {
        if (i > 0) oss << ".";
        oss << oid[i];
    }
    return oss.str();
}

bool SnmpService::isOidPrefix(const std::string& prefix, const std::string& oid) {
    if (oid.size() < prefix.size()) return false;
    if (oid.substr(0, prefix.size()) != prefix) return false;

    // Ensure we're at a boundary
    if (oid.size() > prefix.size() && oid[prefix.size()] != '.') {
        return false;
    }

    return true;
}

} // namespace netpulse::infra
