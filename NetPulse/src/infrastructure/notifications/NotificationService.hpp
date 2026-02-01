#pragma once

#include "core/services/INotificationService.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/notifications/HttpClient.hpp"

#include <QObject>
#include <memory>
#include <mutex>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Notification service for sending alerts via webhooks.
 *
 * Provides webhook-based alert notifications supporting multiple services
 * (Slack, Discord, PagerDuty, custom). Implements core::INotificationService.
 * Supports delivery logging and retry mechanisms.
 */
class NotificationService : public QObject, public core::INotificationService {
    Q_OBJECT

public:
    /**
     * @brief Constructs a NotificationService.
     * @param db Shared pointer to the Database for webhook persistence.
     * @param parent Optional parent QObject for memory management.
     */
    explicit NotificationService(std::shared_ptr<Database> db, QObject* parent = nullptr);

    /**
     * @brief Default destructor.
     */
    ~NotificationService() override = default;

    /**
     * @brief Sends an alert notification to all configured webhooks.
     * @param alert The alert to send.
     * @param hostName Name of the host that generated the alert.
     */
    void sendAlert(const core::Alert& alert, const std::string& hostName) override;

    /**
     * @brief Adds a new webhook configuration.
     * @param config WebhookConfig to add.
     */
    void addWebhook(const core::WebhookConfig& config) override;

    /**
     * @brief Updates an existing webhook configuration.
     * @param config WebhookConfig with updated values (id must be set).
     */
    void updateWebhook(const core::WebhookConfig& config) override;

    /**
     * @brief Removes a webhook configuration.
     * @param webhookId ID of the webhook to remove.
     */
    void removeWebhook(int64_t webhookId) override;

    /**
     * @brief Retrieves a webhook configuration by ID.
     * @param webhookId ID of the webhook.
     * @return WebhookConfig if found, nullopt otherwise.
     */
    std::optional<core::WebhookConfig> getWebhook(int64_t webhookId) const override;

    /**
     * @brief Retrieves all webhook configurations.
     * @return Vector of all WebhookConfig entities.
     */
    std::vector<core::WebhookConfig> getWebhooks() const override;

    /**
     * @brief Enables or disables the notification service.
     * @param enabled True to enable, false to disable.
     */
    void setEnabled(bool enabled) override;

    /**
     * @brief Checks if the notification service is enabled.
     * @return True if enabled, false otherwise.
     */
    bool isEnabled() const override;

    /**
     * @brief Subscribes to notification events.
     * @param callback Function called when notifications are sent.
     */
    void subscribe(NotificationCallback callback) override;

    /**
     * @brief Removes all notification subscribers.
     */
    void unsubscribeAll() override;

    /**
     * @brief Tests a webhook configuration by sending a test message.
     * @param config WebhookConfig to test.
     * @return True if test was successful, false otherwise.
     */
    bool testWebhook(const core::WebhookConfig& config) override;

    /**
     * @brief Retrieves delivery logs for a webhook.
     * @param webhookId ID of the webhook.
     * @param limit Maximum number of logs to return.
     * @return Vector of WebhookDeliveryLog entries.
     */
    std::vector<core::WebhookDeliveryLog> getDeliveryLogs(int64_t webhookId,
                                                           int limit = 100) const override;

    /**
     * @brief Loads webhook configurations from the database.
     */
    void loadWebhooksFromDatabase();

signals:
    /**
     * @brief Emitted when a webhook delivery succeeds.
     * @param webhook The webhook that was used.
     * @param status Delivery status information.
     */
    void webhookDelivered(const core::WebhookConfig& webhook, const core::NotificationStatus& status);

    /**
     * @brief Emitted when a webhook delivery fails.
     * @param webhook The webhook that failed.
     * @param error Error message describing the failure.
     */
    void webhookFailed(const core::WebhookConfig& webhook, const std::string& error);

private:
    void sendToWebhook(const core::WebhookConfig& webhook, const core::Alert& alert,
                       const std::string& hostName, int retryCount = 0);

    void logDelivery(int64_t webhookId, int64_t alertId, bool success, int httpStatus,
                     const std::string& errorMessage);

    std::string buildPayload(const core::WebhookConfig& webhook, const core::Alert& alert,
                             const std::string& hostName) const;

    std::string buildSlackPayload(const core::Alert& alert, const std::string& hostName) const;
    std::string buildDiscordPayload(const core::Alert& alert, const std::string& hostName) const;
    std::string buildPagerDutyPayload(const core::WebhookConfig& webhook, const core::Alert& alert,
                                       const std::string& hostName) const;

    std::map<std::string, std::string> getHeaders(const core::WebhookConfig& webhook) const;

    std::string formatTimestamp(std::chrono::system_clock::time_point tp) const;
    int severityToColor(core::AlertSeverity severity) const;
    std::string severityToPagerDuty(core::AlertSeverity severity) const;

    std::shared_ptr<Database> db_;
    std::unique_ptr<HttpClient> httpClient_;
    std::vector<core::WebhookConfig> webhooks_;
    std::vector<NotificationCallback> subscribers_;
    bool enabled_{true};
    mutable std::mutex mutex_;
};

} // namespace netpulse::infra
