/**
 * @file ISnmpService.hpp
 * @brief Interface for the SNMP monitoring service.
 *
 * This file defines the abstract interface for performing SNMP queries
 * and monitoring SNMP-enabled network devices.
 */

#pragma once

#include "core/types/Host.hpp"
#include "core/types/SnmpTypes.hpp"

#include <functional>
#include <future>
#include <memory>
#include <vector>

namespace netpulse::core {

/**
 * @brief Interface for SNMP monitoring service.
 *
 * Provides methods for performing SNMP operations (GET, GET-NEXT, WALK)
 * and managing continuous monitoring of SNMP-enabled devices.
 */
class ISnmpService {
public:
    /**
     * @brief Callback function type for SNMP result notifications.
     * @param result The result of the SNMP query.
     */
    using SnmpCallback = std::function<void(const SnmpResult&)>;

    virtual ~ISnmpService() = default;

    /**
     * @brief Performs an asynchronous SNMP GET request.
     * @param address IP address or hostname of the SNMP agent.
     * @param oids Vector of OIDs to retrieve.
     * @param config SNMP device configuration (credentials, timeout, etc.).
     * @return Future that will contain the SNMP result.
     */
    virtual std::future<SnmpResult> getAsync(const std::string& address,
                                              const std::vector<std::string>& oids,
                                              const SnmpDeviceConfig& config) = 0;

    /**
     * @brief Performs an asynchronous SNMP GET-NEXT request.
     *
     * GET-NEXT retrieves the next OID after the specified ones, useful
     * for walking MIB trees.
     *
     * @param address IP address or hostname of the SNMP agent.
     * @param oids Vector of OIDs to get the next values after.
     * @param config SNMP device configuration.
     * @return Future that will contain the SNMP result.
     */
    virtual std::future<SnmpResult> getNextAsync(const std::string& address,
                                                  const std::vector<std::string>& oids,
                                                  const SnmpDeviceConfig& config) = 0;

    /**
     * @brief Performs an asynchronous SNMP WALK operation.
     *
     * Walks a subtree of the MIB starting from the specified root OID,
     * retrieving all values underneath.
     *
     * @param address IP address or hostname of the SNMP agent.
     * @param rootOid The root OID of the subtree to walk.
     * @param config SNMP device configuration.
     * @return Future that will contain a vector of all variable bindings.
     */
    virtual std::future<std::vector<SnmpVarBind>> walkAsync(const std::string& address,
                                                             const std::string& rootOid,
                                                             const SnmpDeviceConfig& config) = 0;

    /**
     * @brief Starts periodic SNMP monitoring of a device.
     * @param host The host to monitor.
     * @param config SNMP device configuration including OIDs to poll.
     * @param callback Function to call with each poll result.
     */
    virtual void startMonitoring(const Host& host,
                                  const SnmpDeviceConfig& config,
                                  SnmpCallback callback) = 0;

    /**
     * @brief Stops SNMP monitoring of a specific host.
     * @param hostId ID of the host to stop monitoring.
     */
    virtual void stopMonitoring(int64_t hostId) = 0;

    /**
     * @brief Stops SNMP monitoring of all hosts.
     */
    virtual void stopAllMonitoring() = 0;

    /**
     * @brief Checks if a host is currently being monitored via SNMP.
     * @param hostId ID of the host to check.
     * @return True if the host is being monitored.
     */
    virtual bool isMonitoring(int64_t hostId) const = 0;

    /**
     * @brief Updates the SNMP configuration for a monitored host.
     * @param hostId ID of the host to update.
     * @param config The new SNMP device configuration.
     */
    virtual void updateConfig(int64_t hostId, const SnmpDeviceConfig& config) = 0;

    /**
     * @brief Gets SNMP polling statistics for a monitored host.
     * @param hostId ID of the host.
     * @return Aggregated statistics for the host.
     */
    virtual SnmpStatistics getStatistics(int64_t hostId) const = 0;
};

} // namespace netpulse::core
