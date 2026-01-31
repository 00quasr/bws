#include "core/types/Notification.hpp"

#include <algorithm>

namespace netpulse::core {

std::string WebhookConfig::providerToString() const {
    switch (provider) {
    case WebhookProvider::Slack:
        return "slack";
    case WebhookProvider::Discord:
        return "discord";
    case WebhookProvider::PagerDuty:
        return "pagerduty";
    default:
        return "unknown";
    }
}

WebhookProvider WebhookConfig::providerFromString(const std::string& str) {
    if (str == "slack") {
        return WebhookProvider::Slack;
    }
    if (str == "discord") {
        return WebhookProvider::Discord;
    }
    if (str == "pagerduty") {
        return WebhookProvider::PagerDuty;
    }
    return WebhookProvider::Slack;
}

bool WebhookConfig::matchesAlert(const Alert& alert) const {
    if (!enabled) {
        return false;
    }

    if (!severityFilter.empty()) {
        auto it = std::find(severityFilter.begin(), severityFilter.end(), alert.severity);
        if (it == severityFilter.end()) {
            return false;
        }
    }

    if (!typeFilter.empty()) {
        auto it = std::find(typeFilter.begin(), typeFilter.end(), alert.type);
        if (it == typeFilter.end()) {
            return false;
        }
    }

    return true;
}

} // namespace netpulse::core
