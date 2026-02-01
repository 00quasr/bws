/**
 * @file INotificationService.hpp
 * @brief Interface for the webhook notification service.
 *
 * This file defines the abstract interface for managing webhook notifications,
 * including webhook configuration, alert delivery, and delivery logging.
 */

#pragma once

#include "core/types/Alert.hpp"
#include "core/types/Notification.hpp"

#include <functional>
#include <string>
#include <vector>

namespace netpulse::core {

/**
 * @brief Interface for webhook notification service.
 *
 * Provides methods for sending alerts to configured webhooks, managing
 * webhook configurations, and retrieving delivery logs.
 */
class INotificationService {
public:
    /**
     * @brief Callback function type for notification delivery status.
     * @param webhook The webhook that was used.
     * @param status The delivery status result.
     */
    using NotificationCallback =
        std::function<void(const WebhookConfig& webhook, const NotificationStatus& status)>;

    virtual ~INotificationService() = default;

    /**
     * @brief Sends an alert to all configured and matching webhooks.
     * @param alert The alert to send.
     * @param hostName Name of the host for message formatting.
     */
    virtual void sendAlert(const Alert& alert, const std::string& hostName) = 0;

    /**
     * @brief Adds a new webhook configuration.
     * @param config The webhook configuration to add.
     */
    virtual void addWebhook(const WebhookConfig& config) = 0;

    /**
     * @brief Updates an existing webhook configuration.
     * @param config The updated webhook configuration (matched by ID).
     */
    virtual void updateWebhook(const WebhookConfig& config) = 0;

    /**
     * @brief Removes a webhook configuration.
     * @param webhookId ID of the webhook to remove.
     */
    virtual void removeWebhook(int64_t webhookId) = 0;

    /**
     * @brief Gets a specific webhook configuration.
     * @param webhookId ID of the webhook to retrieve.
     * @return The webhook configuration if found, std::nullopt otherwise.
     */
    virtual std::optional<WebhookConfig> getWebhook(int64_t webhookId) const = 0;

    /**
     * @brief Gets all configured webhooks.
     * @return Vector of all webhook configurations.
     */
    virtual std::vector<WebhookConfig> getWebhooks() const = 0;

    /**
     * @brief Enables or disables the notification service.
     * @param enabled True to enable notifications, false to disable.
     */
    virtual void setEnabled(bool enabled) = 0;

    /**
     * @brief Checks if the notification service is enabled.
     * @return True if notifications are enabled.
     */
    virtual bool isEnabled() const = 0;

    /**
     * @brief Subscribes to notification delivery status updates.
     * @param callback Function to call when a notification is delivered or fails.
     */
    virtual void subscribe(NotificationCallback callback) = 0;

    /**
     * @brief Removes all notification status subscriptions.
     */
    virtual void unsubscribeAll() = 0;

    /**
     * @brief Tests a webhook by sending a test notification.
     * @param config The webhook configuration to test.
     * @return True if the test notification was delivered successfully.
     */
    virtual bool testWebhook(const WebhookConfig& config) = 0;

    /**
     * @brief Gets delivery logs for a specific webhook.
     * @param webhookId ID of the webhook.
     * @param limit Maximum number of log entries to return (default: 100).
     * @return Vector of delivery log entries, ordered by timestamp descending.
     */
    virtual std::vector<WebhookDeliveryLog> getDeliveryLogs(int64_t webhookId, int limit = 100) const = 0;
};

} // namespace netpulse::core
