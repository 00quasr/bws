#pragma once

#include "core/types/Host.hpp"
#include "core/types/SnmpTypes.hpp"

#include <functional>
#include <future>
#include <memory>
#include <vector>

namespace netpulse::core {

class ISnmpService {
public:
    using SnmpCallback = std::function<void(const SnmpResult&)>;

    virtual ~ISnmpService() = default;

    // Single async SNMP GET request
    virtual std::future<SnmpResult> getAsync(const std::string& address,
                                              const std::vector<std::string>& oids,
                                              const SnmpDeviceConfig& config) = 0;

    // SNMP GET-NEXT (for walking MIB trees)
    virtual std::future<SnmpResult> getNextAsync(const std::string& address,
                                                  const std::vector<std::string>& oids,
                                                  const SnmpDeviceConfig& config) = 0;

    // SNMP WALK (iterate through a subtree)
    virtual std::future<std::vector<SnmpVarBind>> walkAsync(const std::string& address,
                                                             const std::string& rootOid,
                                                             const SnmpDeviceConfig& config) = 0;

    // Start periodic monitoring of a device
    virtual void startMonitoring(const Host& host,
                                  const SnmpDeviceConfig& config,
                                  SnmpCallback callback) = 0;

    // Stop monitoring a specific host
    virtual void stopMonitoring(int64_t hostId) = 0;

    // Stop all monitoring
    virtual void stopAllMonitoring() = 0;

    // Check if a host is being monitored
    virtual bool isMonitoring(int64_t hostId) const = 0;

    // Update configuration for a monitored host
    virtual void updateConfig(int64_t hostId, const SnmpDeviceConfig& config) = 0;

    // Get statistics for a monitored host
    virtual SnmpStatistics getStatistics(int64_t hostId) const = 0;
};

} // namespace netpulse::core
