#pragma once

#include "core/services/ISnmpService.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace netpulse::infra {

/**
 * @brief SNMP service for querying and monitoring network devices.
 *
 * Provides asynchronous SNMP v1/v2c/v3 operations including GET, GET-NEXT,
 * and WALK. Supports continuous monitoring of devices with configurable
 * polling intervals. Implements the core::ISnmpService interface.
 *
 * @note This class is non-copyable.
 */
class SnmpService : public core::ISnmpService {
public:
    /**
     * @brief Constructs an SnmpService with the given Asio context.
     * @param context Reference to the AsioContext for async operations.
     */
    explicit SnmpService(AsioContext& context);

    /**
     * @brief Destructor. Stops all monitoring and releases resources.
     */
    ~SnmpService() override;

    SnmpService(const SnmpService&) = delete;
    SnmpService& operator=(const SnmpService&) = delete;

    /**
     * @brief Performs an asynchronous SNMP GET request.
     * @param address Target hostname or IP address.
     * @param oids Vector of OID strings to query.
     * @param config SNMP device configuration (community, version, etc.).
     * @return Future containing the SnmpResult with retrieved values.
     */
    std::future<core::SnmpResult> getAsync(const std::string& address,
                                            const std::vector<std::string>& oids,
                                            const core::SnmpDeviceConfig& config) override;

    /**
     * @brief Performs an asynchronous SNMP GET-NEXT request.
     * @param address Target hostname or IP address.
     * @param oids Vector of OID strings for GET-NEXT operation.
     * @param config SNMP device configuration.
     * @return Future containing the SnmpResult with next OID values.
     */
    std::future<core::SnmpResult> getNextAsync(const std::string& address,
                                                const std::vector<std::string>& oids,
                                                const core::SnmpDeviceConfig& config) override;

    /**
     * @brief Performs an asynchronous SNMP WALK operation.
     * @param address Target hostname or IP address.
     * @param rootOid Root OID to walk from.
     * @param config SNMP device configuration.
     * @return Future containing vector of SnmpVarBind for all values under rootOid.
     */
    std::future<std::vector<core::SnmpVarBind>> walkAsync(const std::string& address,
                                                           const std::string& rootOid,
                                                           const core::SnmpDeviceConfig& config) override;

    /**
     * @brief Starts continuous SNMP monitoring of a host.
     * @param host The host to monitor.
     * @param config SNMP configuration for the device.
     * @param callback Function called with each polling result.
     */
    void startMonitoring(const core::Host& host,
                          const core::SnmpDeviceConfig& config,
                          SnmpCallback callback) override;

    /**
     * @brief Stops SNMP monitoring for a specific host.
     * @param hostId Unique identifier of the host.
     */
    void stopMonitoring(int64_t hostId) override;

    /**
     * @brief Stops SNMP monitoring for all hosts.
     */
    void stopAllMonitoring() override;

    /**
     * @brief Checks if a host is currently being monitored.
     * @param hostId Unique identifier of the host.
     * @return True if monitoring is active, false otherwise.
     */
    bool isMonitoring(int64_t hostId) const override;

    /**
     * @brief Updates the SNMP configuration for a monitored host.
     * @param hostId Unique identifier of the host.
     * @param config New SNMP device configuration.
     */
    void updateConfig(int64_t hostId, const core::SnmpDeviceConfig& config) override;

    /**
     * @brief Retrieves SNMP statistics for a monitored host.
     * @param hostId Unique identifier of the host.
     * @return SnmpStatistics containing success/failure counts and timing info.
     */
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
