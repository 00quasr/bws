#pragma once

#include "core/types/Alert.hpp"
#include "core/types/Notification.hpp"

#include <functional>
#include <string>
#include <vector>

namespace netpulse::core {

class INotificationService {
public:
    using NotificationCallback =
        std::function<void(const WebhookConfig& webhook, const NotificationStatus& status)>;

    virtual ~INotificationService() = default;

    virtual void sendAlert(const Alert& alert, const std::string& hostName) = 0;

    virtual void addWebhook(const WebhookConfig& config) = 0;
    virtual void updateWebhook(const WebhookConfig& config) = 0;
    virtual void removeWebhook(int64_t webhookId) = 0;
    virtual std::optional<WebhookConfig> getWebhook(int64_t webhookId) const = 0;
    virtual std::vector<WebhookConfig> getWebhooks() const = 0;

    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;

    virtual void subscribe(NotificationCallback callback) = 0;
    virtual void unsubscribeAll() = 0;

    virtual bool testWebhook(const WebhookConfig& config) = 0;

    virtual std::vector<WebhookDeliveryLog> getDeliveryLogs(int64_t webhookId, int limit = 100) const = 0;
};

} // namespace netpulse::core
