#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"
#include "core/types/Notification.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/notifications/NotificationService.hpp"

#include <QCoreApplication>
#include <filesystem>

using namespace netpulse::core;
using namespace netpulse::infra;

namespace {

class TestDatabase {
public:
    TestDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_notification_test.db") {
        std::filesystem::remove(dbPath_);
        db_ = std::make_shared<Database>(dbPath_.string());
        db_->runMigrations();
    }

    ~TestDatabase() {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::shared_ptr<Database> get() { return db_; }

private:
    std::filesystem::path dbPath_;
    std::shared_ptr<Database> db_;
};

Alert createTestAlert(AlertType type, AlertSeverity severity) {
    Alert alert;
    alert.id = 1;
    alert.hostId = 1;
    alert.type = type;
    alert.severity = severity;
    alert.title = "Test Alert";
    alert.message = "This is a test alert message";
    alert.timestamp = std::chrono::system_clock::now();
    alert.acknowledged = false;
    return alert;
}

} // namespace

TEST_CASE("WebhookConfig provider conversion", "[Notification]") {
    WebhookConfig config;

    SECTION("Slack provider") {
        config.provider = WebhookProvider::Slack;
        CHECK(config.providerToString() == "slack");
        CHECK(WebhookConfig::providerFromString("slack") == WebhookProvider::Slack);
    }

    SECTION("Discord provider") {
        config.provider = WebhookProvider::Discord;
        CHECK(config.providerToString() == "discord");
        CHECK(WebhookConfig::providerFromString("discord") == WebhookProvider::Discord);
    }

    SECTION("PagerDuty provider") {
        config.provider = WebhookProvider::PagerDuty;
        CHECK(config.providerToString() == "pagerduty");
        CHECK(WebhookConfig::providerFromString("pagerduty") == WebhookProvider::PagerDuty);
    }

    SECTION("Unknown provider defaults to Slack") {
        CHECK(WebhookConfig::providerFromString("unknown") == WebhookProvider::Slack);
    }
}

TEST_CASE("WebhookConfig matchesAlert filtering", "[Notification]") {
    WebhookConfig config;
    config.enabled = true;

    SECTION("Disabled webhook never matches") {
        config.enabled = false;
        auto alert = createTestAlert(AlertType::HostDown, AlertSeverity::Critical);
        CHECK_FALSE(config.matchesAlert(alert));
    }

    SECTION("Empty filters match all alerts") {
        auto alert = createTestAlert(AlertType::HostDown, AlertSeverity::Critical);
        CHECK(config.matchesAlert(alert));

        alert = createTestAlert(AlertType::HighLatency, AlertSeverity::Warning);
        CHECK(config.matchesAlert(alert));

        alert = createTestAlert(AlertType::HostRecovered, AlertSeverity::Info);
        CHECK(config.matchesAlert(alert));
    }

    SECTION("Severity filter works") {
        config.severityFilter = {AlertSeverity::Critical};

        auto criticalAlert = createTestAlert(AlertType::HostDown, AlertSeverity::Critical);
        CHECK(config.matchesAlert(criticalAlert));

        auto warningAlert = createTestAlert(AlertType::HighLatency, AlertSeverity::Warning);
        CHECK_FALSE(config.matchesAlert(warningAlert));
    }

    SECTION("Multiple severity filters work") {
        config.severityFilter = {AlertSeverity::Critical, AlertSeverity::Warning};

        auto criticalAlert = createTestAlert(AlertType::HostDown, AlertSeverity::Critical);
        CHECK(config.matchesAlert(criticalAlert));

        auto warningAlert = createTestAlert(AlertType::HighLatency, AlertSeverity::Warning);
        CHECK(config.matchesAlert(warningAlert));

        auto infoAlert = createTestAlert(AlertType::HostRecovered, AlertSeverity::Info);
        CHECK_FALSE(config.matchesAlert(infoAlert));
    }

    SECTION("Type filter works") {
        config.typeFilter = {AlertType::HostDown};

        auto hostDownAlert = createTestAlert(AlertType::HostDown, AlertSeverity::Critical);
        CHECK(config.matchesAlert(hostDownAlert));

        auto latencyAlert = createTestAlert(AlertType::HighLatency, AlertSeverity::Warning);
        CHECK_FALSE(config.matchesAlert(latencyAlert));
    }

    SECTION("Combined severity and type filters work") {
        config.severityFilter = {AlertSeverity::Critical};
        config.typeFilter = {AlertType::HostDown, AlertType::PacketLoss};

        auto criticalHostDown = createTestAlert(AlertType::HostDown, AlertSeverity::Critical);
        CHECK(config.matchesAlert(criticalHostDown));

        auto warningHostDown = createTestAlert(AlertType::HostDown, AlertSeverity::Warning);
        CHECK_FALSE(config.matchesAlert(warningHostDown));

        auto criticalLatency = createTestAlert(AlertType::HighLatency, AlertSeverity::Critical);
        CHECK_FALSE(config.matchesAlert(criticalLatency));
    }
}

TEST_CASE("NotificationService webhook CRUD operations", "[NotificationService]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QCoreApplication app(argc, argv);

    TestDatabase testDb;
    NotificationService service(testDb.get());

    SECTION("Add and retrieve webhook") {
        WebhookConfig config;
        config.name = "test-slack";
        config.provider = WebhookProvider::Slack;
        config.url = "https://hooks.slack.com/services/test";
        config.enabled = true;
        config.timeoutMs = 5000;
        config.maxRetries = 3;

        service.addWebhook(config);

        auto webhooks = service.getWebhooks();
        REQUIRE(webhooks.size() == 1);
        CHECK(webhooks[0].name == "test-slack");
        CHECK(webhooks[0].provider == WebhookProvider::Slack);
        CHECK(webhooks[0].url == "https://hooks.slack.com/services/test");
        CHECK(webhooks[0].enabled);
    }

    SECTION("Update webhook") {
        WebhookConfig config;
        config.name = "test-discord";
        config.provider = WebhookProvider::Discord;
        config.url = "https://discord.com/api/webhooks/test";
        config.enabled = true;

        service.addWebhook(config);

        auto webhooks = service.getWebhooks();
        REQUIRE(webhooks.size() == 1);

        auto updated = webhooks[0];
        updated.url = "https://discord.com/api/webhooks/updated";
        updated.enabled = false;
        service.updateWebhook(updated);

        webhooks = service.getWebhooks();
        REQUIRE(webhooks.size() == 1);
        CHECK(webhooks[0].url == "https://discord.com/api/webhooks/updated");
        CHECK_FALSE(webhooks[0].enabled);
    }

    SECTION("Remove webhook") {
        WebhookConfig config;
        config.name = "test-pagerduty";
        config.provider = WebhookProvider::PagerDuty;
        config.url = "https://events.pagerduty.com/v2/enqueue";
        config.apiToken = "test-token";

        service.addWebhook(config);

        auto webhooks = service.getWebhooks();
        REQUIRE(webhooks.size() == 1);

        service.removeWebhook(webhooks[0].id);

        webhooks = service.getWebhooks();
        CHECK(webhooks.empty());
    }

    SECTION("Get webhook by ID") {
        WebhookConfig config;
        config.name = "test-webhook";
        config.provider = WebhookProvider::Slack;
        config.url = "https://hooks.slack.com/test";

        service.addWebhook(config);

        auto webhooks = service.getWebhooks();
        REQUIRE(webhooks.size() == 1);

        auto retrieved = service.getWebhook(webhooks[0].id);
        REQUIRE(retrieved.has_value());
        CHECK(retrieved->name == "test-webhook");

        auto notFound = service.getWebhook(9999);
        CHECK_FALSE(notFound.has_value());
    }

    SECTION("Multiple webhooks") {
        WebhookConfig slack;
        slack.name = "slack-webhook";
        slack.provider = WebhookProvider::Slack;
        slack.url = "https://hooks.slack.com/test";

        WebhookConfig discord;
        discord.name = "discord-webhook";
        discord.provider = WebhookProvider::Discord;
        discord.url = "https://discord.com/api/webhooks/test";

        WebhookConfig pagerduty;
        pagerduty.name = "pagerduty-webhook";
        pagerduty.provider = WebhookProvider::PagerDuty;
        pagerduty.url = "https://events.pagerduty.com/v2/enqueue";
        pagerduty.apiToken = "token";

        service.addWebhook(slack);
        service.addWebhook(discord);
        service.addWebhook(pagerduty);

        auto webhooks = service.getWebhooks();
        CHECK(webhooks.size() == 3);
    }
}

TEST_CASE("NotificationService enabled state", "[NotificationService]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QCoreApplication app(argc, argv);

    TestDatabase testDb;
    NotificationService service(testDb.get());

    SECTION("Default state is enabled") {
        CHECK(service.isEnabled());
    }

    SECTION("Can disable and enable") {
        service.setEnabled(false);
        CHECK_FALSE(service.isEnabled());

        service.setEnabled(true);
        CHECK(service.isEnabled());
    }
}

TEST_CASE("NotificationService webhook filters are persisted", "[NotificationService]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QCoreApplication app(argc, argv);

    TestDatabase testDb;
    NotificationService service(testDb.get());

    WebhookConfig config;
    config.name = "filtered-webhook";
    config.provider = WebhookProvider::Slack;
    config.url = "https://hooks.slack.com/test";
    config.severityFilter = {AlertSeverity::Critical, AlertSeverity::Warning};
    config.typeFilter = {AlertType::HostDown, AlertType::PacketLoss};

    service.addWebhook(config);

    auto webhooks = service.getWebhooks();
    REQUIRE(webhooks.size() == 1);

    auto& retrieved = webhooks[0];
    REQUIRE(retrieved.severityFilter.size() == 2);
    CHECK(retrieved.severityFilter[0] == AlertSeverity::Critical);
    CHECK(retrieved.severityFilter[1] == AlertSeverity::Warning);

    REQUIRE(retrieved.typeFilter.size() == 2);
    CHECK(retrieved.typeFilter[0] == AlertType::HostDown);
    CHECK(retrieved.typeFilter[1] == AlertType::PacketLoss);
}

TEST_CASE("NotificationService subscription", "[NotificationService]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QCoreApplication app(argc, argv);

    TestDatabase testDb;
    NotificationService service(testDb.get());

    bool callbackInvoked = false;
    service.subscribe([&callbackInvoked](const WebhookConfig&, const NotificationStatus&) {
        callbackInvoked = true;
    });

    service.unsubscribeAll();
    CHECK_FALSE(callbackInvoked);
}
