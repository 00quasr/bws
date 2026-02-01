/**
 * @file AlertsViewModel.hpp
 * @brief ViewModel for alert management and display.
 *
 * This file defines the AlertsViewModel class which implements the IAlertService
 * interface and provides alert management functionality for the MVVM architecture.
 */

#pragma once

#include "core/services/IAlertService.hpp"
#include "core/services/INotificationService.hpp"
#include "core/types/Alert.hpp"
#include "infrastructure/database/MetricsRepository.hpp"

#include <QObject>
#include <memory>
#include <mutex>
#include <vector>

namespace netpulse::viewmodels {

/**
 * @brief ViewModel for managing monitoring alerts.
 *
 * Implements the IAlertService interface to provide alert generation based
 * on configurable thresholds. Tracks host failures, generates alerts, and
 * supports optional notification delivery. Thread-safe for concurrent access.
 */
class AlertsViewModel : public QObject, public core::IAlertService {
    Q_OBJECT

public:
    /**
     * @brief Constructs an AlertsViewModel.
     * @param db Shared pointer to the database connection.
     * @param notificationService Optional notification service for alert delivery.
     * @param parent Optional parent QObject for Qt ownership.
     */
    explicit AlertsViewModel(std::shared_ptr<infra::Database> db,
                             std::shared_ptr<core::INotificationService> notificationService = nullptr,
                             QObject* parent = nullptr);

    /**
     * @brief Sets the thresholds for alert generation.
     * @param thresholds The new threshold configuration.
     */
    void setThresholds(const core::AlertThresholds& thresholds) override;

    /**
     * @brief Gets the current alert thresholds.
     * @return The current threshold configuration.
     */
    core::AlertThresholds getThresholds() const override;

    /**
     * @brief Processes a ping result and generates alerts if necessary.
     * @param hostId ID of the host that was pinged.
     * @param hostName Name of the host for alert messages.
     * @param result The ping result to process.
     */
    void processResult(int64_t hostId, const std::string& hostName,
                       const core::PingResult& result) override;

    /**
     * @brief Subscribes to alert notifications.
     * @param callback Function to call when a new alert is generated.
     */
    void subscribe(AlertCallback callback) override;

    /**
     * @brief Removes all alert subscriptions.
     */
    void unsubscribeAll() override;

    /**
     * @brief Gets the most recent alerts.
     * @param limit Maximum number of alerts to return (default: 100).
     * @return Vector of recent alerts, ordered by timestamp descending.
     */
    std::vector<core::Alert> getRecentAlerts(int limit = 100) const override;

    /**
     * @brief Gets alerts matching the specified filter criteria.
     * @param filter Filter criteria to apply.
     * @param limit Maximum number of alerts to return (default: 100).
     * @return Vector of matching alerts.
     */
    std::vector<core::Alert> getFilteredAlerts(const core::AlertFilter& filter,
                                                int limit = 100) const override;

    /**
     * @brief Acknowledges an alert.
     * @param alertId ID of the alert to acknowledge.
     */
    void acknowledgeAlert(int64_t alertId) override;

    /**
     * @brief Clears all alerts from the system.
     */
    void clearAlerts() override;

    /**
     * @brief Gets the count of unacknowledged alerts.
     * @return Number of alerts not yet acknowledged.
     */
    int unacknowledgedCount() const;

    /**
     * @brief Acknowledges all pending alerts.
     */
    void acknowledgeAll();

signals:
    /**
     * @brief Emitted when a new alert is triggered.
     * @param alert The alert that was generated.
     */
    void alertTriggered(const core::Alert& alert);

    /**
     * @brief Emitted when an alert is acknowledged.
     * @param alertId ID of the acknowledged alert.
     */
    void alertAcknowledged(int64_t alertId);

    /**
     * @brief Emitted when all alerts are cleared.
     */
    void alertsCleared();

private:
    void triggerAlert(const core::Alert& alert);

    std::string getHostName(int64_t hostId) const;

    std::shared_ptr<infra::Database> db_;
    std::unique_ptr<infra::MetricsRepository> metricsRepo_;
    std::shared_ptr<core::INotificationService> notificationService_;
    core::AlertThresholds thresholds_;
    std::vector<AlertCallback> subscribers_;
    mutable std::mutex mutex_;

    // Track consecutive failures per host
    std::map<int64_t, int> consecutiveFailures_;
    std::map<int64_t, bool> hostWasDown_;
    std::map<int64_t, std::string> hostNames_;
};

} // namespace netpulse::viewmodels
