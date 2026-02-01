#include <catch2/catch_test_macros.hpp>

#include "core/types/Notification.hpp"

using namespace netpulse::core;

TEST_CASE("WebhookConfig default values", "[Notification][WebhookConfig]") {
    WebhookConfig config;

    REQUIRE(config.id == 0);
    REQUIRE(config.name.empty());
    REQUIRE(config.provider == WebhookProvider::Slack);
    REQUIRE(config.url.empty());
    REQUIRE(config.apiToken.empty());
    REQUIRE(config.enabled == true);
    REQUIRE(config.severityFilter.empty());
    REQUIRE(config.typeFilter.empty());
    REQUIRE(config.timeoutMs == 5000);
    REQUIRE(config.maxRetries == 3);
}

TEST_CASE("WebhookConfig provider string conversion", "[Notification][WebhookConfig]") {
    SECTION("providerToString") {
        WebhookConfig config;

        config.provider = WebhookProvider::Slack;
        REQUIRE(config.providerToString() == "slack");

        config.provider = WebhookProvider::Discord;
        REQUIRE(config.providerToString() == "discord");

        config.provider = WebhookProvider::PagerDuty;
        REQUIRE(config.providerToString() == "pagerduty");
    }

    SECTION("providerFromString") {
        REQUIRE(WebhookConfig::providerFromString("slack") == WebhookProvider::Slack);
        REQUIRE(WebhookConfig::providerFromString("discord") == WebhookProvider::Discord);
        REQUIRE(WebhookConfig::providerFromString("pagerduty") == WebhookProvider::PagerDuty);
        REQUIRE(WebhookConfig::providerFromString("invalid") == WebhookProvider::Slack);
    }
}

TEST_CASE("WebhookConfig matchesAlert", "[Notification][WebhookConfig]") {
    WebhookConfig config;
    config.enabled = true;

    Alert alert;
    alert.type = AlertType::HostDown;
    alert.severity = AlertSeverity::Critical;

    SECTION("Disabled webhook does not match") {
        config.enabled = false;
        REQUIRE_FALSE(config.matchesAlert(alert));
    }

    SECTION("Enabled webhook with empty filters matches all") {
        config.enabled = true;
        config.severityFilter.clear();
        config.typeFilter.clear();
        REQUIRE(config.matchesAlert(alert));
    }

    SECTION("Severity filter - matches") {
        config.severityFilter = {AlertSeverity::Critical, AlertSeverity::Warning};
        REQUIRE(config.matchesAlert(alert));
    }

    SECTION("Severity filter - does not match") {
        config.severityFilter = {AlertSeverity::Info, AlertSeverity::Warning};
        REQUIRE_FALSE(config.matchesAlert(alert));
    }

    SECTION("Type filter - matches") {
        config.typeFilter = {AlertType::HostDown, AlertType::HighLatency};
        REQUIRE(config.matchesAlert(alert));
    }

    SECTION("Type filter - does not match") {
        config.typeFilter = {AlertType::HighLatency, AlertType::PacketLoss};
        REQUIRE_FALSE(config.matchesAlert(alert));
    }

    SECTION("Both filters - matches") {
        config.severityFilter = {AlertSeverity::Critical};
        config.typeFilter = {AlertType::HostDown};
        REQUIRE(config.matchesAlert(alert));
    }

    SECTION("Both filters - severity matches, type does not") {
        config.severityFilter = {AlertSeverity::Critical};
        config.typeFilter = {AlertType::HighLatency};
        REQUIRE_FALSE(config.matchesAlert(alert));
    }

    SECTION("Both filters - type matches, severity does not") {
        config.severityFilter = {AlertSeverity::Info};
        config.typeFilter = {AlertType::HostDown};
        REQUIRE_FALSE(config.matchesAlert(alert));
    }

    SECTION("Match different alert types") {
        config.severityFilter.clear();
        config.typeFilter = {AlertType::HostRecovered, AlertType::ScanComplete};

        alert.type = AlertType::HostRecovered;
        REQUIRE(config.matchesAlert(alert));

        alert.type = AlertType::ScanComplete;
        REQUIRE(config.matchesAlert(alert));

        alert.type = AlertType::HostDown;
        REQUIRE_FALSE(config.matchesAlert(alert));
    }
}

TEST_CASE("WebhookConfig equality", "[Notification][WebhookConfig]") {
    WebhookConfig config1;
    config1.id = 1;
    config1.name = "Test Webhook";
    config1.provider = WebhookProvider::Discord;
    config1.url = "https://discord.com/api/webhooks/123";
    config1.enabled = true;

    WebhookConfig config2 = config1;
    REQUIRE(config1 == config2);

    config2.name = "Different";
    REQUIRE_FALSE(config1 == config2);
}

TEST_CASE("WebhookDeliveryLog default values", "[Notification][DeliveryLog]") {
    WebhookDeliveryLog log;

    REQUIRE(log.id == 0);
    REQUIRE(log.webhookId == 0);
    REQUIRE(log.alertId == 0);
    REQUIRE(log.success == false);
    REQUIRE(log.httpStatus == 0);
    REQUIRE(log.errorMessage.empty());
}

TEST_CASE("NotificationStatus default values", "[Notification][Status]") {
    NotificationStatus status;

    REQUIRE(status.result == NotificationResult::Failed);
    REQUIRE(status.webhookName.empty());
    REQUIRE(status.errorMessage.empty());
    REQUIRE(status.httpStatus == 0);
    REQUIRE(status.retryCount == 0);
}

TEST_CASE("NotificationResult enum values", "[Notification]") {
    REQUIRE(static_cast<int>(NotificationResult::Success) != static_cast<int>(NotificationResult::Failed));
    REQUIRE(static_cast<int>(NotificationResult::Retrying) != static_cast<int>(NotificationResult::Skipped));

    NotificationStatus status;

    status.result = NotificationResult::Success;
    REQUIRE(status.result == NotificationResult::Success);

    status.result = NotificationResult::Failed;
    REQUIRE(status.result == NotificationResult::Failed);

    status.result = NotificationResult::Retrying;
    REQUIRE(status.result == NotificationResult::Retrying);

    status.result = NotificationResult::Skipped;
    REQUIRE(status.result == NotificationResult::Skipped);
}
