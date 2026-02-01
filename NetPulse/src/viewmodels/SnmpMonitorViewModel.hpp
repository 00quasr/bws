/**
 * @file SnmpMonitorViewModel.hpp
 * @brief ViewModel for SNMP device monitoring.
 *
 * This file defines the SnmpMonitorViewModel class which provides operations
 * for SNMP device configuration, polling, and statistics in the MVVM architecture.
 */

#pragma once

#include "core/types/Host.hpp"
#include "core/types/SnmpTypes.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/SnmpRepository.hpp"
#include "infrastructure/network/SnmpService.hpp"

#include <QObject>
#include <memory>
#include <vector>

namespace netpulse::viewmodels {

/**
 * @brief ViewModel for SNMP-based device monitoring.
 *
 * Provides operations for configuring SNMP device polling, retrieving
 * SNMP results and statistics, and managing device monitoring state.
 * Supports both scheduled polling and ad-hoc queries.
 */
class SnmpMonitorViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a SnmpMonitorViewModel.
     * @param db Shared pointer to the database connection.
     * @param snmpService Shared pointer to the SNMP service for polling.
     * @param parent Optional parent QObject for Qt ownership.
     */
    explicit SnmpMonitorViewModel(std::shared_ptr<infra::Database> db,
                                   std::shared_ptr<infra::SnmpService> snmpService,
                                   QObject* parent = nullptr);

    /**
     * @brief Destroys the SnmpMonitorViewModel.
     */
    ~SnmpMonitorViewModel() override;

    /**
     * @brief Starts SNMP monitoring for all configured devices.
     */
    void startMonitoring();

    /**
     * @brief Stops SNMP monitoring for all devices.
     */
    void stopMonitoring();

    /**
     * @brief Starts SNMP monitoring for a specific host.
     * @param hostId ID of the host to start monitoring.
     */
    void startMonitoringHost(int64_t hostId);

    /**
     * @brief Stops SNMP monitoring for a specific host.
     * @param hostId ID of the host to stop monitoring.
     */
    void stopMonitoringHost(int64_t hostId);

    /**
     * @brief Adds a new SNMP device configuration.
     * @param config The SNMP device configuration.
     * @return The ID of the newly created configuration.
     */
    int64_t addDeviceConfig(const core::SnmpDeviceConfig& config);

    /**
     * @brief Updates an existing SNMP device configuration.
     * @param config The configuration with updated fields.
     */
    void updateDeviceConfig(const core::SnmpDeviceConfig& config);

    /**
     * @brief Removes an SNMP device configuration.
     * @param configId ID of the configuration to remove.
     */
    void removeDeviceConfig(int64_t configId);

    /**
     * @brief Gets the SNMP configuration for a host.
     * @param hostId ID of the host to query.
     * @return The device configuration if found, std::nullopt otherwise.
     */
    std::optional<core::SnmpDeviceConfig> getDeviceConfig(int64_t hostId) const;

    /**
     * @brief Gets all SNMP device configurations.
     * @return Vector of all device configurations.
     */
    std::vector<core::SnmpDeviceConfig> getAllDeviceConfigs() const;

    /**
     * @brief Gets recent SNMP poll results for a host.
     * @param hostId ID of the host to query.
     * @param limit Maximum number of results to return (default: 100).
     * @return Vector of recent SNMP results, ordered by timestamp descending.
     */
    std::vector<core::SnmpResult> getRecentResults(int64_t hostId, int limit = 100) const;

    /**
     * @brief Gets SNMP statistics for a host.
     * @param hostId ID of the host to query.
     * @return Aggregated SNMP statistics.
     */
    core::SnmpStatistics getStatistics(int64_t hostId) const;

    /**
     * @brief Gets historical values for a specific OID.
     * @param hostId ID of the host to query.
     * @param oid The OID to get history for.
     * @param limit Maximum number of values to return (default: 100).
     * @return Map of OID to timestamped values.
     */
    std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>>
        getOidHistory(int64_t hostId, const std::string& oid, int limit = 100) const;

    /**
     * @brief Performs an ad-hoc SNMP query on a device.
     * @param hostId ID of the host to query.
     * @param oids List of OIDs to poll.
     */
    void queryDevice(int64_t hostId, const std::vector<std::string>& oids);

    /**
     * @brief Gets the total number of configured SNMP devices.
     * @return Total device count.
     */
    int deviceCount() const;

    /**
     * @brief Gets the number of devices currently being polled.
     * @return Count of devices in active polling state.
     */
    int devicesPolling() const;

    /**
     * @brief Gets the number of devices with recent errors.
     * @return Count of devices with polling errors.
     */
    int devicesWithErrors() const;

signals:
    /**
     * @brief Emitted when an SNMP poll result is received.
     * @param hostId ID of the host that was polled.
     * @param result The SNMP result data.
     */
    void snmpResultReceived(int64_t hostId, const core::SnmpResult& result);

    /**
     * @brief Emitted when a device's status changes.
     * @param hostId ID of the device whose status changed.
     * @param success True if last poll succeeded, false otherwise.
     */
    void deviceStatusChanged(int64_t hostId, bool success);

    /**
     * @brief Emitted when a new device configuration is added.
     * @param configId ID of the newly added configuration.
     */
    void configAdded(int64_t configId);

    /**
     * @brief Emitted when a device configuration is updated.
     * @param configId ID of the updated configuration.
     */
    void configUpdated(int64_t configId);

    /**
     * @brief Emitted when a device configuration is removed.
     * @param configId ID of the removed configuration.
     */
    void configRemoved(int64_t configId);

    /**
     * @brief Emitted when an ad-hoc query completes.
     * @param hostId ID of the host that was queried.
     * @param result The query result data.
     */
    void queryCompleted(int64_t hostId, const core::SnmpResult& result);

private slots:
    void onSnmpResult(int64_t hostId, const core::SnmpResult& result);

private:
    void processResult(int64_t hostId, const core::SnmpResult& result);

    std::shared_ptr<infra::Database> db_;
    std::shared_ptr<infra::SnmpService> snmpService_;
    std::unique_ptr<infra::HostRepository> hostRepo_;
    std::unique_ptr<infra::SnmpRepository> snmpRepo_;

    std::map<int64_t, int> consecutiveFailures_;
    std::map<int64_t, bool> lastSuccessState_;
    int consecutiveFailuresThreshold_{3};
};

} // namespace netpulse::viewmodels
