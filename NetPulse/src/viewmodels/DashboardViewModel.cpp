#include "viewmodels/DashboardViewModel.hpp"

#include <QMetaObject>
#include <spdlog/spdlog.h>

namespace netpulse::viewmodels {

DashboardViewModel::DashboardViewModel(std::shared_ptr<infra::Database> db,
                                       std::shared_ptr<infra::PingService> pingService,
                                       QObject* parent)
    : QObject(parent), db_(std::move(db)), pingService_(std::move(pingService)) {
    hostRepo_ = std::make_unique<infra::HostRepository>(db_);
    metricsRepo_ = std::make_unique<infra::MetricsRepository>(db_);
}

DashboardViewModel::~DashboardViewModel() {
    stopMonitoring();
}

void DashboardViewModel::startMonitoring() {
    auto hosts = hostRepo_->findEnabled();

    for (const auto& host : hosts) {
        auto callback = [this, hostId = host.id](const core::PingResult& result) {
            QMetaObject::invokeMethod(
                this, [this, hostId, result]() { onPingResult(hostId, result); },
                Qt::QueuedConnection);
        };

        pingService_->startMonitoring(host, callback);
    }

    spdlog::info("Started monitoring {} hosts", hosts.size());
}

void DashboardViewModel::stopMonitoring() {
    pingService_->stopAllMonitoring();
    spdlog::info("Stopped all host monitoring");
}

std::vector<core::Host> DashboardViewModel::getHosts() const {
    return hostRepo_->findAll();
}

std::vector<core::PingResult> DashboardViewModel::getRecentResults(int64_t hostId,
                                                                    int limit) const {
    return metricsRepo_->getPingResults(hostId, limit);
}

core::PingStatistics DashboardViewModel::getStatistics(int64_t hostId) const {
    return metricsRepo_->getStatistics(hostId);
}

std::vector<core::NetworkInterface> DashboardViewModel::getNetworkInterfaces() const {
    return core::NetworkInterfaceEnumerator::enumerate();
}

int DashboardViewModel::hostCount() const {
    return hostRepo_->count();
}

int DashboardViewModel::hostsUp() const {
    auto hosts = hostRepo_->findAll();
    return static_cast<int>(std::count_if(hosts.begin(), hosts.end(), [](const auto& h) {
        return h.status == core::HostStatus::Up;
    }));
}

int DashboardViewModel::hostsDown() const {
    auto hosts = hostRepo_->findAll();
    return static_cast<int>(std::count_if(hosts.begin(), hosts.end(), [](const auto& h) {
        return h.status == core::HostStatus::Down;
    }));
}

void DashboardViewModel::onPingResult(int64_t hostId, const core::PingResult& result) {
    // Store result in database
    core::PingResult storedResult = result;
    storedResult.hostId = hostId;
    metricsRepo_->insertPingResult(storedResult);

    // Update host status
    updateHostStatus(hostId, result);

    // Update last checked time
    hostRepo_->updateLastChecked(hostId);

    emit pingResultReceived(hostId, result);
}

void DashboardViewModel::updateHostStatus(int64_t hostId, const core::PingResult& result) {
    auto host = hostRepo_->findById(hostId);
    if (!host) {
        return;
    }

    core::HostStatus newStatus = host->status;

    if (result.success) {
        consecutiveFailures_[hostId] = 0;

        double latencyMs = result.latencyMs();
        if (latencyMs >= host->criticalThresholdMs) {
            newStatus = core::HostStatus::Warning;
        } else if (latencyMs >= host->warningThresholdMs) {
            newStatus = core::HostStatus::Warning;
        } else {
            newStatus = core::HostStatus::Up;
        }
    } else {
        int failures = ++consecutiveFailures_[hostId];
        if (failures >= consecutiveFailuresThreshold_) {
            newStatus = core::HostStatus::Down;
        }
    }

    if (newStatus != host->status) {
        hostRepo_->updateStatus(hostId, newStatus);
        emit hostStatusChanged(hostId, newStatus);
        spdlog::info("Host {} status changed to {}", host->name,
                     core::Host{.status = newStatus}.statusToString());
    }
}

} // namespace netpulse::viewmodels
