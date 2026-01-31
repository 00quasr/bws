#pragma once

#include "core/services/IAlertService.hpp"
#include "core/types/Alert.hpp"
#include "infrastructure/database/MetricsRepository.hpp"

#include <QObject>
#include <memory>
#include <mutex>
#include <vector>

namespace netpulse::viewmodels {

class AlertsViewModel : public QObject, public core::IAlertService {
    Q_OBJECT

public:
    explicit AlertsViewModel(std::shared_ptr<infra::Database> db, QObject* parent = nullptr);

    // IAlertService implementation
    void setThresholds(const core::AlertThresholds& thresholds) override;
    core::AlertThresholds getThresholds() const override;

    void processResult(int64_t hostId, const std::string& hostName,
                       const core::PingResult& result) override;

    void subscribe(AlertCallback callback) override;
    void unsubscribeAll() override;

    std::vector<core::Alert> getRecentAlerts(int limit = 100) const override;
    std::vector<core::Alert> getFilteredAlerts(const core::AlertFilter& filter,
                                                int limit = 100) const override;
    void acknowledgeAlert(int64_t alertId) override;
    void clearAlerts() override;

    // Additional methods
    int unacknowledgedCount() const;
    void acknowledgeAll();

signals:
    void alertTriggered(const core::Alert& alert);
    void alertAcknowledged(int64_t alertId);
    void alertsCleared();

private:
    void triggerAlert(const core::Alert& alert);

    std::shared_ptr<infra::Database> db_;
    std::unique_ptr<infra::MetricsRepository> metricsRepo_;
    core::AlertThresholds thresholds_;
    std::vector<AlertCallback> subscribers_;
    mutable std::mutex mutex_;

    // Track consecutive failures per host
    std::map<int64_t, int> consecutiveFailures_;
    std::map<int64_t, bool> hostWasDown_;
};

} // namespace netpulse::viewmodels
