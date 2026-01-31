#pragma once

#include "core/services/ISnmpService.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace netpulse::infra {

class SnmpService : public core::ISnmpService {
public:
    explicit SnmpService(AsioContext& context);
    ~SnmpService() override;

    SnmpService(const SnmpService&) = delete;
    SnmpService& operator=(const SnmpService&) = delete;

    // ISnmpService interface
    std::future<core::SnmpResult> getAsync(const std::string& address,
                                            const std::vector<std::string>& oids,
                                            const core::SnmpDeviceConfig& config) override;

    std::future<core::SnmpResult> getNextAsync(const std::string& address,
                                                const std::vector<std::string>& oids,
                                                const core::SnmpDeviceConfig& config) override;

    std::future<std::vector<core::SnmpVarBind>> walkAsync(const std::string& address,
                                                           const std::string& rootOid,
                                                           const core::SnmpDeviceConfig& config) override;

    void startMonitoring(const core::Host& host,
                          const core::SnmpDeviceConfig& config,
                          SnmpCallback callback) override;

    void stopMonitoring(int64_t hostId) override;
    void stopAllMonitoring() override;
    bool isMonitoring(int64_t hostId) const override;
    void updateConfig(int64_t hostId, const core::SnmpDeviceConfig& config) override;
    core::SnmpStatistics getStatistics(int64_t hostId) const override;

private:
    // SNMP PDU types
    enum class PduType : uint8_t {
        GetRequest = 0xA0,
        GetNextRequest = 0xA1,
        GetResponse = 0xA2,
        SetRequest = 0xA3,
        GetBulkRequest = 0xA5
    };

    // Internal monitoring state
    struct MonitoredDevice {
        core::Host host;
        core::SnmpDeviceConfig config;
        SnmpCallback callback;
        std::shared_ptr<asio::steady_timer> timer;
        std::atomic<bool> active{true};
        core::SnmpStatistics statistics;
    };

    // Schedule next poll for a monitored device
    void scheduleNextPoll(std::shared_ptr<MonitoredDevice> device);

    // Perform SNMP operation synchronously
    core::SnmpResult performSnmpGet(const std::string& address,
                                     const std::vector<std::string>& oids,
                                     const core::SnmpDeviceConfig& config,
                                     PduType pduType);

    // SNMP packet encoding/decoding
    std::vector<uint8_t> buildSnmpPacket(const std::vector<std::string>& oids,
                                          const core::SnmpDeviceConfig& config,
                                          PduType pduType,
                                          int32_t requestId);

    core::SnmpResult parseSnmpResponse(const std::vector<uint8_t>& response,
                                        const core::SnmpDeviceConfig& config);

    // BER/ASN.1 encoding helpers
    static std::vector<uint8_t> encodeLength(size_t length);
    static std::vector<uint8_t> encodeInteger(int32_t value);
    static std::vector<uint8_t> encodeOctetString(const std::string& str);
    static std::vector<uint8_t> encodeOid(const std::string& oid);
    static std::vector<uint8_t> encodeNull();
    static std::vector<uint8_t> encodeSequence(const std::vector<uint8_t>& content);

    // BER/ASN.1 decoding helpers
    static size_t decodeLength(const uint8_t* data, size_t& offset);
    static int32_t decodeInteger(const uint8_t* data, size_t length);
    static std::string decodeOctetString(const uint8_t* data, size_t length);
    static std::string decodeOid(const uint8_t* data, size_t length);
    static uint64_t decodeCounter64(const uint8_t* data, size_t length);

    // OID manipulation
    static std::vector<uint32_t> parseOidString(const std::string& oid);
    static std::string oidVectorToString(const std::vector<uint32_t>& oid);
    static bool isOidPrefix(const std::string& prefix, const std::string& oid);

    // SNMP v3 helpers
    std::vector<uint8_t> buildSnmpV3Packet(const std::vector<std::string>& oids,
                                            const core::SnmpDeviceConfig& config,
                                            PduType pduType,
                                            int32_t requestId);

    AsioContext& context_;
    std::map<int64_t, std::shared_ptr<MonitoredDevice>> monitoredDevices_;
    mutable std::mutex mutex_;
    std::atomic<int32_t> requestIdCounter_{1};
};

} // namespace netpulse::infra
