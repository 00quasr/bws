#pragma once

#include "core/services/INotificationService.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/notifications/HttpClient.hpp"

#include <QObject>
#include <memory>
#include <mutex>
#include <vector>

namespace netpulse::infra {

class NotificationService : public QObject, public core::INotificationService {
    Q_OBJECT

public:
    explicit NotificationService(std::shared_ptr<Database> db, QObject* parent = nullptr);
    ~NotificationService() override = default;

    // INotificationService implementation
    void sendAlert(const core::Alert& alert, const std::string& hostName) override;

    void addWebhook(const core::WebhookConfig& config) override;
    void updateWebhook(const core::WebhookConfig& config) override;
    void removeWebhook(int64_t webhookId) override;
    std::optional<core::WebhookConfig> getWebhook(int64_t webhookId) const override;
    std::vector<core::WebhookConfig> getWebhooks() const override;

    void setEnabled(bool enabled) override;
    bool isEnabled() const override;

    void subscribe(NotificationCallback callback) override;
    void unsubscribeAll() override;

    bool testWebhook(const core::WebhookConfig& config) override;

    std::vector<core::WebhookDeliveryLog> getDeliveryLogs(int64_t webhookId,
                                                           int limit = 100) const override;

    void loadWebhooksFromDatabase();

signals:
    void webhookDelivered(const core::WebhookConfig& webhook, const core::NotificationStatus& status);
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
