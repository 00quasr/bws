#pragma once

#include "core/types/Alert.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::core {

enum class WebhookProvider : int { Slack = 0, Discord = 1, PagerDuty = 2 };

struct WebhookConfig {
    int64_t id{0};
    std::string name;
    WebhookProvider provider{WebhookProvider::Slack};
    std::string url;
    std::string apiToken;
    bool enabled{true};
    std::vector<AlertSeverity> severityFilter;
    std::vector<AlertType> typeFilter;
    int timeoutMs{5000};
    int maxRetries{3};

    [[nodiscard]] std::string providerToString() const;
    static WebhookProvider providerFromString(const std::string& str);

    [[nodiscard]] bool matchesAlert(const Alert& alert) const;

    bool operator==(const WebhookConfig& other) const = default;
};

struct WebhookDeliveryLog {
    int64_t id{0};
    int64_t webhookId{0};
    int64_t alertId{0};
    bool success{false};
    int httpStatus{0};
    std::string errorMessage;
    std::chrono::system_clock::time_point sentAt;
};

enum class NotificationResult { Success, Failed, Retrying, Skipped };

struct NotificationStatus {
    NotificationResult result{NotificationResult::Failed};
    std::string webhookName;
    std::string errorMessage;
    int httpStatus{0};
    int retryCount{0};
};

} // namespace netpulse::core
