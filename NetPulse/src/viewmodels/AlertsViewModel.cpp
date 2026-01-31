#include "viewmodels/AlertsViewModel.hpp"

#include <QMetaObject>
#include <spdlog/spdlog.h>

namespace netpulse::viewmodels {

AlertsViewModel::AlertsViewModel(std::shared_ptr<infra::Database> db,
                                 std::shared_ptr<core::INotificationService> notificationService,
                                 QObject* parent)
    : QObject(parent), db_(std::move(db)), notificationService_(std::move(notificationService)) {
    metricsRepo_ = std::make_unique<infra::MetricsRepository>(db_);
}

void AlertsViewModel::setThresholds(const core::AlertThresholds& thresholds) {
    std::lock_guard lock(mutex_);
    thresholds_ = thresholds;
}

core::AlertThresholds AlertsViewModel::getThresholds() const {
    std::lock_guard lock(mutex_);
    return thresholds_;
}

void AlertsViewModel::processResult(int64_t hostId, const std::string& hostName,
                                    const core::PingResult& result) {
    std::lock_guard lock(mutex_);

    hostNames_[hostId] = hostName;

    if (result.success) {
        double latencyMs = result.latencyMs();

        // Check for high latency
        if (latencyMs >= thresholds_.latencyCriticalMs) {
            core::Alert alert;
            alert.hostId = hostId;
            alert.type = core::AlertType::HighLatency;
            alert.severity = core::AlertSeverity::Critical;
            alert.title = "Critical Latency";
            alert.message = hostName + " latency is " + std::to_string(static_cast<int>(latencyMs)) +
                            "ms (threshold: " + std::to_string(thresholds_.latencyCriticalMs) + "ms)";
            alert.timestamp = std::chrono::system_clock::now();
            triggerAlert(alert);
        } else if (latencyMs >= thresholds_.latencyWarningMs) {
            core::Alert alert;
            alert.hostId = hostId;
            alert.type = core::AlertType::HighLatency;
            alert.severity = core::AlertSeverity::Warning;
            alert.title = "High Latency Warning";
            alert.message = hostName + " latency is " + std::to_string(static_cast<int>(latencyMs)) +
                            "ms (threshold: " + std::to_string(thresholds_.latencyWarningMs) + "ms)";
            alert.timestamp = std::chrono::system_clock::now();
            triggerAlert(alert);
        }

        // Check if host recovered
        if (hostWasDown_[hostId]) {
            hostWasDown_[hostId] = false;
            consecutiveFailures_[hostId] = 0;

            core::Alert alert;
            alert.hostId = hostId;
            alert.type = core::AlertType::HostRecovered;
            alert.severity = core::AlertSeverity::Info;
            alert.title = "Host Recovered";
            alert.message = hostName + " is now responding";
            alert.timestamp = std::chrono::system_clock::now();
            triggerAlert(alert);
        }

        consecutiveFailures_[hostId] = 0;
    } else {
        // Track consecutive failures
        int failures = ++consecutiveFailures_[hostId];

        if (failures >= thresholds_.consecutiveFailuresForDown && !hostWasDown_[hostId]) {
            hostWasDown_[hostId] = true;

            core::Alert alert;
            alert.hostId = hostId;
            alert.type = core::AlertType::HostDown;
            alert.severity = core::AlertSeverity::Critical;
            alert.title = "Host Down";
            alert.message = hostName + " is not responding after " + std::to_string(failures) +
                            " consecutive failures";
            alert.timestamp = std::chrono::system_clock::now();
            triggerAlert(alert);
        }
    }
}

void AlertsViewModel::subscribe(AlertCallback callback) {
    std::lock_guard lock(mutex_);
    subscribers_.push_back(std::move(callback));
}

void AlertsViewModel::unsubscribeAll() {
    std::lock_guard lock(mutex_);
    subscribers_.clear();
}

std::vector<core::Alert> AlertsViewModel::getRecentAlerts(int limit) const {
    return metricsRepo_->getAlerts(limit);
}

std::vector<core::Alert> AlertsViewModel::getFilteredAlerts(const core::AlertFilter& filter,
                                                             int limit) const {
    return metricsRepo_->getAlertsFiltered(filter, limit);
}

void AlertsViewModel::acknowledgeAlert(int64_t alertId) {
    metricsRepo_->acknowledgeAlert(alertId);
    emit alertAcknowledged(alertId);
}

void AlertsViewModel::clearAlerts() {
    db_->execute("DELETE FROM alerts");
    emit alertsCleared();
}

int AlertsViewModel::unacknowledgedCount() const {
    auto alerts = metricsRepo_->getUnacknowledgedAlerts();
    return static_cast<int>(alerts.size());
}

void AlertsViewModel::acknowledgeAll() {
    metricsRepo_->acknowledgeAll();
}

void AlertsViewModel::triggerAlert(const core::Alert& alert) {
    // Store in database
    metricsRepo_->insertAlert(alert);

    // Send webhook notifications
    if (notificationService_) {
        std::string hostName = getHostName(alert.hostId);
        notificationService_->sendAlert(alert, hostName);
    }

    // Notify subscribers
    for (const auto& callback : subscribers_) {
        callback(alert);
    }

    // Emit Qt signal (thread-safe)
    QMetaObject::invokeMethod(this, [this, alert]() { emit alertTriggered(alert); },
                              Qt::QueuedConnection);

    spdlog::info("Alert triggered: {} - {}", alert.title, alert.message);
}

std::string AlertsViewModel::getHostName(int64_t hostId) const {
    auto it = hostNames_.find(hostId);
    if (it != hostNames_.end()) {
        return it->second;
    }

    auto results = db_->query("SELECT name FROM hosts WHERE id = ?", hostId);
    if (!results.empty()) {
        return results[0].at("name").get<std::string>();
    }
    return "Unknown Host";
}

} // namespace netpulse::viewmodels
