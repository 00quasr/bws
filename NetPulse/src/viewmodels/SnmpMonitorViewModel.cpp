#include "viewmodels/SnmpMonitorViewModel.hpp"

#include <QMetaObject>
#include <spdlog/spdlog.h>

namespace netpulse::viewmodels {

SnmpMonitorViewModel::SnmpMonitorViewModel(std::shared_ptr<infra::Database> db,
                                             std::shared_ptr<infra::SnmpService> snmpService,
                                             QObject* parent)
    : QObject(parent), db_(std::move(db)), snmpService_(std::move(snmpService)) {
    hostRepo_ = std::make_unique<infra::HostRepository>(db_);
    snmpRepo_ = std::make_unique<infra::SnmpRepository>(db_);
}

SnmpMonitorViewModel::~SnmpMonitorViewModel() {
    stopMonitoring();
}

void SnmpMonitorViewModel::startMonitoring() {
    auto configs = snmpRepo_->getEnabledDeviceConfigs();

    for (const auto& config : configs) {
        auto host = hostRepo_->findById(config.hostId);
        if (!host || !host->enabled) {
            continue;
        }

        auto callback = [this, hostId = config.hostId](const core::SnmpResult& result) {
            QMetaObject::invokeMethod(
                this, [this, hostId, result]() { onSnmpResult(hostId, result); },
                Qt::QueuedConnection);
        };

        snmpService_->startMonitoring(*host, config, callback);
    }

    spdlog::info("Started SNMP monitoring for {} devices", configs.size());
}

void SnmpMonitorViewModel::stopMonitoring() {
    snmpService_->stopAllMonitoring();
    spdlog::info("Stopped all SNMP monitoring");
}

void SnmpMonitorViewModel::startMonitoringHost(int64_t hostId) {
    auto config = snmpRepo_->getDeviceConfigByHostId(hostId);
    if (!config) {
        spdlog::warn("No SNMP config found for host ID {}", hostId);
        return;
    }

    auto host = hostRepo_->findById(hostId);
    if (!host) {
        spdlog::warn("Host ID {} not found", hostId);
        return;
    }

    auto callback = [this, hostId](const core::SnmpResult& result) {
        QMetaObject::invokeMethod(
            this, [this, hostId, result]() { onSnmpResult(hostId, result); },
            Qt::QueuedConnection);
    };

    snmpService_->startMonitoring(*host, *config, callback);
    spdlog::info("Started SNMP monitoring for host {} ({})", host->name, host->address);
}

void SnmpMonitorViewModel::stopMonitoringHost(int64_t hostId) {
    snmpService_->stopMonitoring(hostId);
    spdlog::info("Stopped SNMP monitoring for host ID {}", hostId);
}

int64_t SnmpMonitorViewModel::addDeviceConfig(const core::SnmpDeviceConfig& config) {
    core::SnmpDeviceConfig newConfig = config;
    newConfig.createdAt = std::chrono::system_clock::now();
    int64_t id = snmpRepo_->insertDeviceConfig(newConfig);
    emit configAdded(id);
    spdlog::info("Added SNMP config {} for host ID {}", id, config.hostId);
    return id;
}

void SnmpMonitorViewModel::updateDeviceConfig(const core::SnmpDeviceConfig& config) {
    snmpRepo_->updateDeviceConfig(config);

    // Update service if monitoring
    if (snmpService_->isMonitoring(config.hostId)) {
        snmpService_->updateConfig(config.hostId, config);
    }

    emit configUpdated(config.id);
    spdlog::info("Updated SNMP config {}", config.id);
}

void SnmpMonitorViewModel::removeDeviceConfig(int64_t configId) {
    auto config = snmpRepo_->getDeviceConfig(configId);
    if (config) {
        // Stop monitoring if active
        if (snmpService_->isMonitoring(config->hostId)) {
            snmpService_->stopMonitoring(config->hostId);
        }

        snmpRepo_->deleteDeviceConfig(configId);
        emit configRemoved(configId);
        spdlog::info("Removed SNMP config {}", configId);
    }
}

std::optional<core::SnmpDeviceConfig> SnmpMonitorViewModel::getDeviceConfig(int64_t hostId) const {
    return snmpRepo_->getDeviceConfigByHostId(hostId);
}

std::vector<core::SnmpDeviceConfig> SnmpMonitorViewModel::getAllDeviceConfigs() const {
    return snmpRepo_->getAllDeviceConfigs();
}

std::vector<core::SnmpResult> SnmpMonitorViewModel::getRecentResults(int64_t hostId, int limit) const {
    return snmpRepo_->getResults(hostId, limit);
}

core::SnmpStatistics SnmpMonitorViewModel::getStatistics(int64_t hostId) const {
    return snmpRepo_->getStatistics(hostId);
}

std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>>
SnmpMonitorViewModel::getOidHistory(int64_t hostId, const std::string& oid, int limit) const {
    return snmpRepo_->getOidHistory(hostId, oid, limit);
}

void SnmpMonitorViewModel::queryDevice(int64_t hostId, const std::vector<std::string>& oids) {
    auto config = snmpRepo_->getDeviceConfigByHostId(hostId);
    if (!config) {
        spdlog::warn("No SNMP config found for host ID {}", hostId);
        return;
    }

    auto host = hostRepo_->findById(hostId);
    if (!host) {
        spdlog::warn("Host ID {} not found", hostId);
        return;
    }

    // Create a modified config with the requested OIDs
    core::SnmpDeviceConfig queryConfig = *config;
    queryConfig.oids = oids;

    // Perform async query
    auto future = snmpService_->getAsync(host->address, oids, queryConfig);

    // Process result in background and signal when done
    std::thread([this, hostId, future = std::move(future)]() mutable {
        auto result = future.get();
        result.hostId = hostId;

        QMetaObject::invokeMethod(
            this, [this, hostId, result]() {
                snmpRepo_->insertResult(result);
                emit queryCompleted(hostId, result);
            },
            Qt::QueuedConnection);
    }).detach();
}

int SnmpMonitorViewModel::deviceCount() const {
    return static_cast<int>(snmpRepo_->getAllDeviceConfigs().size());
}

int SnmpMonitorViewModel::devicesPolling() const {
    auto configs = snmpRepo_->getEnabledDeviceConfigs();
    int count = 0;
    for (const auto& config : configs) {
        if (snmpService_->isMonitoring(config.hostId)) {
            ++count;
        }
    }
    return count;
}

int SnmpMonitorViewModel::devicesWithErrors() const {
    int count = 0;
    for (const auto& [hostId, success] : lastSuccessState_) {
        if (!success) {
            ++count;
        }
    }
    return count;
}

void SnmpMonitorViewModel::onSnmpResult(int64_t hostId, const core::SnmpResult& result) {
    processResult(hostId, result);
    emit snmpResultReceived(hostId, result);
}

void SnmpMonitorViewModel::processResult(int64_t hostId, const core::SnmpResult& result) {
    // Store result in database
    core::SnmpResult storedResult = result;
    storedResult.hostId = hostId;
    snmpRepo_->insertResult(storedResult);

    // Track consecutive failures
    bool wasSuccessful = lastSuccessState_.count(hostId) ? lastSuccessState_[hostId] : true;

    if (result.success) {
        consecutiveFailures_[hostId] = 0;
        lastSuccessState_[hostId] = true;

        if (!wasSuccessful) {
            emit deviceStatusChanged(hostId, true);
            spdlog::info("SNMP device {} is now responding", hostId);
        }
    } else {
        int failures = ++consecutiveFailures_[hostId];
        if (failures >= consecutiveFailuresThreshold_) {
            lastSuccessState_[hostId] = false;

            if (wasSuccessful) {
                emit deviceStatusChanged(hostId, false);
                spdlog::warn("SNMP device {} is not responding: {}", hostId, result.errorMessage);
            }
        }
    }

    // Update last polled time in config
    auto config = snmpRepo_->getDeviceConfigByHostId(hostId);
    if (config) {
        core::SnmpDeviceConfig updatedConfig = *config;
        updatedConfig.lastPolled = result.timestamp;
        snmpRepo_->updateDeviceConfig(updatedConfig);
    }
}

} // namespace netpulse::viewmodels
