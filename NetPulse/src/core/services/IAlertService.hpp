/**
 * @file IAlertService.hpp
 * @brief Interface for the alert management service.
 *
 * This file defines the abstract interface for managing alerts generated
 * by the monitoring system, including threshold configuration and alert retrieval.
 */

#pragma once

#include "core/types/Alert.hpp"
#include "core/types/PingResult.hpp"

#include <functional>
#include <vector>

namespace netpulse::core {

/**
 * @brief Interface for alert management service.
 *
 * Provides methods for configuring alert thresholds, processing monitoring
 * results to generate alerts, and managing alert subscriptions.
 */
class IAlertService {
public:
    /**
     * @brief Callback function type for alert notifications.
     * @param alert The alert that was generated.
     */
    using AlertCallback = std::function<void(const Alert&)>;

    virtual ~IAlertService() = default;

    /**
     * @brief Sets the thresholds for alert generation.
     * @param thresholds The new threshold configuration.
     */
    virtual void setThresholds(const AlertThresholds& thresholds) = 0;

    /**
     * @brief Gets the current alert thresholds.
     * @return The current threshold configuration.
     */
    virtual AlertThresholds getThresholds() const = 0;

    /**
     * @brief Processes a ping result and generates alerts if necessary.
     * @param hostId ID of the host that was pinged.
     * @param hostName Name of the host for alert messages.
     * @param result The ping result to process.
     */
    virtual void processResult(int64_t hostId, const std::string& hostName,
                               const PingResult& result) = 0;

    /**
     * @brief Subscribes to alert notifications.
     * @param callback Function to call when a new alert is generated.
     */
    virtual void subscribe(AlertCallback callback) = 0;

    /**
     * @brief Removes all alert subscriptions.
     */
    virtual void unsubscribeAll() = 0;

    /**
     * @brief Gets the most recent alerts.
     * @param limit Maximum number of alerts to return (default: 100).
     * @return Vector of recent alerts, ordered by timestamp descending.
     */
    virtual std::vector<Alert> getRecentAlerts(int limit = 100) const = 0;

    /**
     * @brief Gets alerts matching the specified filter criteria.
     * @param filter Filter criteria to apply.
     * @param limit Maximum number of alerts to return (default: 100).
     * @return Vector of matching alerts.
     */
    virtual std::vector<Alert> getFilteredAlerts(const AlertFilter& filter,
                                                  int limit = 100) const = 0;

    /**
     * @brief Acknowledges an alert.
     * @param alertId ID of the alert to acknowledge.
     */
    virtual void acknowledgeAlert(int64_t alertId) = 0;

    /**
     * @brief Clears all alerts from the system.
     */
    virtual void clearAlerts() = 0;
};

} // namespace netpulse::core
