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

class SnmpMonitorViewModel : public QObject {
    Q_OBJECT

public:
    explicit SnmpMonitorViewModel(std::shared_ptr<infra::Database> db,
                                   std::shared_ptr<infra::SnmpService> snmpService,
                                   QObject* parent = nullptr);
    ~SnmpMonitorViewModel() override;

    // Monitoring control
    void startMonitoring();
    void stopMonitoring();
    void startMonitoringHost(int64_t hostId);
    void stopMonitoringHost(int64_t hostId);

    // Device configuration
    int64_t addDeviceConfig(const core::SnmpDeviceConfig& config);
    void updateDeviceConfig(const core::SnmpDeviceConfig& config);
    void removeDeviceConfig(int64_t configId);
    std::optional<core::SnmpDeviceConfig> getDeviceConfig(int64_t hostId) const;
    std::vector<core::SnmpDeviceConfig> getAllDeviceConfigs() const;

    // Results and statistics
    std::vector<core::SnmpResult> getRecentResults(int64_t hostId, int limit = 100) const;
    core::SnmpStatistics getStatistics(int64_t hostId) const;
    std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>>
        getOidHistory(int64_t hostId, const std::string& oid, int limit = 100) const;

    // Single query (ad-hoc polling)
    void queryDevice(int64_t hostId, const std::vector<std::string>& oids);

    // Device counts
    int deviceCount() const;
    int devicesPolling() const;
    int devicesWithErrors() const;

signals:
    void snmpResultReceived(int64_t hostId, const core::SnmpResult& result);
    void deviceStatusChanged(int64_t hostId, bool success);
    void configAdded(int64_t configId);
    void configUpdated(int64_t configId);
    void configRemoved(int64_t configId);
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
