#include "viewmodels/HostMonitorViewModel.hpp"

#include <spdlog/spdlog.h>

namespace netpulse::viewmodels {

HostMonitorViewModel::HostMonitorViewModel(std::shared_ptr<infra::Database> db,
                                           std::shared_ptr<infra::PingService> pingService,
                                           QObject* parent)
    : QObject(parent), db_(std::move(db)), pingService_(std::move(pingService)) {
    hostRepo_ = std::make_unique<infra::HostRepository>(db_);
    metricsRepo_ = std::make_unique<infra::MetricsRepository>(db_);
}

int64_t HostMonitorViewModel::addHost(const std::string& name, const std::string& address) {
    core::Host host;
    host.name = name;
    host.address = address;
    host.createdAt = std::chrono::system_clock::now();
    host.status = core::HostStatus::Unknown;
    host.enabled = true;

    int64_t id = hostRepo_->insert(host);
    spdlog::info("Added host: {} ({})", name, address);

    emit hostAdded(id);
    return id;
}

void HostMonitorViewModel::updateHost(const core::Host& host) {
    hostRepo_->update(host);
    spdlog::info("Updated host: {} ({})", host.name, host.address);
    emit hostUpdated(host.id);
}

void HostMonitorViewModel::removeHost(int64_t id) {
    pingService_->stopMonitoring(id);
    hostRepo_->remove(id);
    spdlog::info("Removed host: {}", id);
    emit hostRemoved(id);
}

std::optional<core::Host> HostMonitorViewModel::getHost(int64_t id) const {
    return hostRepo_->findById(id);
}

std::vector<core::Host> HostMonitorViewModel::getAllHosts() const {
    return hostRepo_->findAll();
}

void HostMonitorViewModel::startMonitoringHost(int64_t hostId) {
    auto host = hostRepo_->findById(hostId);
    if (!host) {
        spdlog::warn("Cannot start monitoring: host {} not found", hostId);
        return;
    }

    pingService_->startMonitoring(*host, [](const core::PingResult&) {
        // Results are handled by DashboardViewModel
    });

    spdlog::info("Started monitoring host: {}", host->name);
    emit monitoringStarted(hostId);
}

void HostMonitorViewModel::stopMonitoringHost(int64_t hostId) {
    pingService_->stopMonitoring(hostId);
    spdlog::info("Stopped monitoring host: {}", hostId);
    emit monitoringStopped(hostId);
}

void HostMonitorViewModel::enableHost(int64_t hostId, bool enabled) {
    auto host = hostRepo_->findById(hostId);
    if (!host) {
        return;
    }

    host->enabled = enabled;
    hostRepo_->update(*host);

    if (!enabled) {
        pingService_->stopMonitoring(hostId);
    }

    spdlog::info("Host {} {}", host->name, enabled ? "enabled" : "disabled");
    emit hostUpdated(hostId);
}

std::string HostMonitorViewModel::exportHostData(int64_t hostId, const std::string& format) const {
    if (format == "json") {
        return metricsRepo_->exportToJson(hostId);
    } else if (format == "csv") {
        return metricsRepo_->exportToCsv(hostId);
    }
    return {};
}

void HostMonitorViewModel::clearHostHistory(int64_t hostId) {
    // Delete all ping results for this host
    db_->execute("DELETE FROM ping_results WHERE host_id = " + std::to_string(hostId));
    spdlog::info("Cleared history for host: {}", hostId);
}

} // namespace netpulse::viewmodels
