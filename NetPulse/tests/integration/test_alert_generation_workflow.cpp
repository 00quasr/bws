#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"
#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "viewmodels/AlertsViewModel.hpp"

#include <QCoreApplication>
#include <chrono>
#include <filesystem>
#include <thread>

using namespace netpulse::core;
using namespace netpulse::infra;
using namespace netpulse::viewmodels;

namespace {

class IntegrationTestDatabase {
public:
    IntegrationTestDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_alert_integration_test.db") {
        std::filesystem::remove(dbPath_);
        db_ = std::make_shared<Database>(dbPath_.string());
        db_->runMigrations();
    }

    ~IntegrationTestDatabase() {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::shared_ptr<Database> get() { return db_; }

private:
    std::filesystem::path dbPath_;
    std::shared_ptr<Database> db_;
};

Host createTestHost(const std::string& name, const std::string& address) {
    Host host;
    host.name = name;
    host.address = address;
    host.pingIntervalSeconds = 30;
    host.warningThresholdMs = 100;
    host.criticalThresholdMs = 500;
    host.status = HostStatus::Unknown;
    host.enabled = true;
    host.createdAt = std::chrono::system_clock::now();
    return host;
}

PingResult createSuccessfulPing(int64_t hostId, double latencyMs) {
    PingResult result;
    result.hostId = hostId;
    result.timestamp = std::chrono::system_clock::now();
    result.latency = std::chrono::microseconds(static_cast<int64_t>(latencyMs * 1000));
    result.success = true;
    result.ttl = 64;
    return result;
}

PingResult createFailedPing(int64_t hostId) {
    PingResult result;
    result.hostId = hostId;
    result.timestamp = std::chrono::system_clock::now();
    result.latency = std::chrono::microseconds(0);
    result.success = false;
    result.errorMessage = "Host unreachable";
    return result;
}

} // namespace

// =============================================================================
// Alert Generation Workflow Integration Tests
// =============================================================================

TEST_CASE("Alert workflow - complete latency alert lifecycle",
          "[Integration][Alert][Latency]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t hostId = hostRepo.insert(createTestHost("Latency Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    alertsVm.setThresholds(thresholds);

    SECTION("Normal latency does not trigger alert") {
        alertsVm.processResult(hostId, "Latency Host", createSuccessfulPing(hostId, 50.0));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.empty());

        auto dbAlerts = metricsRepo.getAlerts(100);
        REQUIRE(dbAlerts.empty());
    }

    SECTION("Warning latency triggers warning alert and persists to database") {
        alertsVm.processResult(hostId, "Latency Host", createSuccessfulPing(hostId, 150.0));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HighLatency);
        REQUIRE(alerts[0].severity == AlertSeverity::Warning);
        REQUIRE(alerts[0].hostId == hostId);

        // Verify database persistence
        auto dbAlerts = metricsRepo.getAlerts(100);
        REQUIRE(dbAlerts.size() == 1);
        REQUIRE(dbAlerts[0].type == AlertType::HighLatency);
        REQUIRE(dbAlerts[0].severity == AlertSeverity::Warning);
    }

    SECTION("Critical latency triggers critical alert") {
        alertsVm.processResult(hostId, "Latency Host", createSuccessfulPing(hostId, 600.0));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HighLatency);
        REQUIRE(alerts[0].severity == AlertSeverity::Critical);

        auto dbAlerts = metricsRepo.getAlerts(100);
        REQUIRE(dbAlerts.size() == 1);
        REQUIRE(dbAlerts[0].severity == AlertSeverity::Critical);
    }

    SECTION("Latency escalation from warning to critical") {
        // First warning
        alertsVm.processResult(hostId, "Latency Host", createSuccessfulPing(hostId, 150.0));

        auto warningAlerts = alertsVm.getRecentAlerts(100);
        REQUIRE(warningAlerts.size() == 1);
        REQUIRE(warningAlerts[0].severity == AlertSeverity::Warning);

        // Then critical
        alertsVm.processResult(hostId, "Latency Host", createSuccessfulPing(hostId, 600.0));

        auto allAlerts = alertsVm.getRecentAlerts(100);
        REQUIRE(allAlerts.size() == 2);

        // Most recent first
        REQUIRE(allAlerts[0].severity == AlertSeverity::Critical);
        REQUIRE(allAlerts[1].severity == AlertSeverity::Warning);
    }
}

TEST_CASE("Alert workflow - host down detection and recovery",
          "[Integration][Alert][HostDown]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t hostId = hostRepo.insert(createTestHost("Down Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    alertsVm.setThresholds(thresholds);

    SECTION("Single failure does not trigger host down") {
        alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }

    SECTION("Consecutive failures trigger host down alert") {
        for (int i = 0; i < 3; ++i) {
            alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));
        }

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HostDown);
        REQUIRE(alerts[0].severity == AlertSeverity::Critical);

        // Verify persistence
        auto dbAlerts = metricsRepo.getAlerts(100);
        REQUIRE(dbAlerts.size() == 1);
        REQUIRE(dbAlerts[0].type == AlertType::HostDown);
    }

    SECTION("Recovery after host down triggers recovery alert") {
        // Host goes down
        for (int i = 0; i < 3; ++i) {
            alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));
        }

        // Host recovers
        alertsVm.processResult(hostId, "Down Host", createSuccessfulPing(hostId, 50.0));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 2);

        // Most recent is recovery
        REQUIRE(alerts[0].type == AlertType::HostRecovered);
        REQUIRE(alerts[0].severity == AlertSeverity::Info);

        // Previous is down
        REQUIRE(alerts[1].type == AlertType::HostDown);
        REQUIRE(alerts[1].severity == AlertSeverity::Critical);
    }

    SECTION("Complete down-recovery-down cycle") {
        // First down
        for (int i = 0; i < 3; ++i) {
            alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));
        }

        // Recovery
        alertsVm.processResult(hostId, "Down Host", createSuccessfulPing(hostId, 50.0));

        // Down again
        for (int i = 0; i < 3; ++i) {
            alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));
        }

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 3);

        REQUIRE(alerts[0].type == AlertType::HostDown);     // Second down
        REQUIRE(alerts[1].type == AlertType::HostRecovered); // Recovery
        REQUIRE(alerts[2].type == AlertType::HostDown);     // First down
    }

    SECTION("Failure counter resets on success") {
        // Two failures (not enough)
        alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));
        alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));

        // Success resets counter
        alertsVm.processResult(hostId, "Down Host", createSuccessfulPing(hostId, 50.0));

        // Two more failures
        alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));
        alertsVm.processResult(hostId, "Down Host", createFailedPing(hostId));

        // Should have no alerts (counter was reset)
        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }
}

TEST_CASE("Alert workflow - subscriber notifications",
          "[Integration][Alert][Subscriber]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t hostId = hostRepo.insert(createTestHost("Subscriber Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    alertsVm.setThresholds(thresholds);

    SECTION("Subscribers receive alert notifications") {
        std::vector<Alert> receivedAlerts;
        alertsVm.subscribe([&receivedAlerts](const Alert& alert) {
            receivedAlerts.push_back(alert);
        });

        alertsVm.processResult(hostId, "Subscriber Host", createSuccessfulPing(hostId, 600.0));

        REQUIRE(receivedAlerts.size() == 1);
        REQUIRE(receivedAlerts[0].type == AlertType::HighLatency);
        REQUIRE(receivedAlerts[0].severity == AlertSeverity::Critical);
    }

    SECTION("Multiple subscribers all receive notifications") {
        std::vector<Alert> sub1Alerts;
        std::vector<Alert> sub2Alerts;
        std::vector<Alert> sub3Alerts;

        alertsVm.subscribe([&sub1Alerts](const Alert& alert) {
            sub1Alerts.push_back(alert);
        });
        alertsVm.subscribe([&sub2Alerts](const Alert& alert) {
            sub2Alerts.push_back(alert);
        });
        alertsVm.subscribe([&sub3Alerts](const Alert& alert) {
            sub3Alerts.push_back(alert);
        });

        alertsVm.processResult(hostId, "Subscriber Host", createSuccessfulPing(hostId, 600.0));

        REQUIRE(sub1Alerts.size() == 1);
        REQUIRE(sub2Alerts.size() == 1);
        REQUIRE(sub3Alerts.size() == 1);
    }

    SECTION("Unsubscribe stops notifications") {
        std::vector<Alert> receivedAlerts;
        alertsVm.subscribe([&receivedAlerts](const Alert& alert) {
            receivedAlerts.push_back(alert);
        });

        alertsVm.processResult(hostId, "Subscriber Host", createSuccessfulPing(hostId, 600.0));
        REQUIRE(receivedAlerts.size() == 1);

        alertsVm.unsubscribeAll();

        alertsVm.processResult(hostId, "Subscriber Host", createSuccessfulPing(hostId, 700.0));
        REQUIRE(receivedAlerts.size() == 1); // No new alerts received
    }
}

TEST_CASE("Alert workflow - acknowledgment workflow",
          "[Integration][Alert][Acknowledgment]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t hostId = hostRepo.insert(createTestHost("Ack Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    alertsVm.setThresholds(thresholds);

    // Generate some alerts
    alertsVm.processResult(hostId, "Ack Host", createSuccessfulPing(hostId, 150.0));
    alertsVm.processResult(hostId, "Ack Host", createSuccessfulPing(hostId, 600.0));

    SECTION("Initial alerts are unacknowledged") {
        REQUIRE(alertsVm.unacknowledgedCount() == 2);
    }

    SECTION("Acknowledge single alert") {
        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 2);

        alertsVm.acknowledgeAlert(alerts[0].id);

        REQUIRE(alertsVm.unacknowledgedCount() == 1);

        // Verify in database
        AlertFilter filterAck;
        filterAck.acknowledged = true;
        auto acknowledgedAlerts = metricsRepo.getAlertsFiltered(filterAck, 100);
        REQUIRE(acknowledgedAlerts.size() == 1);
    }

    SECTION("Acknowledge all alerts") {
        alertsVm.acknowledgeAll();

        REQUIRE(alertsVm.unacknowledgedCount() == 0);

        auto unacknowledged = metricsRepo.getUnacknowledgedAlerts();
        REQUIRE(unacknowledged.empty());
    }

    SECTION("Filter alerts by acknowledged status") {
        auto alerts = alertsVm.getRecentAlerts(100);
        alertsVm.acknowledgeAlert(alerts[0].id);

        AlertFilter filterAck;
        filterAck.acknowledged = true;
        auto acknowledged = alertsVm.getFilteredAlerts(filterAck, 100);
        REQUIRE(acknowledged.size() == 1);

        AlertFilter filterUnack;
        filterUnack.acknowledged = false;
        auto unacknowledged = alertsVm.getFilteredAlerts(filterUnack, 100);
        REQUIRE(unacknowledged.size() == 1);
    }
}

TEST_CASE("Alert workflow - filtering and querying",
          "[Integration][Alert][Filter]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t hostId = hostRepo.insert(createTestHost("Filter Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 2;
    alertsVm.setThresholds(thresholds);

    // Generate diverse alerts
    alertsVm.processResult(hostId, "Filter Host", createSuccessfulPing(hostId, 150.0)); // Warning
    alertsVm.processResult(hostId, "Filter Host", createSuccessfulPing(hostId, 600.0)); // Critical
    for (int i = 0; i < 2; ++i) {
        alertsVm.processResult(hostId, "Filter Host", createFailedPing(hostId));
    } // HostDown
    alertsVm.processResult(hostId, "Filter Host", createSuccessfulPing(hostId, 50.0)); // Recovery

    SECTION("Filter by severity") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Warning;

        auto warningAlerts = alertsVm.getFilteredAlerts(filter, 100);
        REQUIRE(warningAlerts.size() == 1);
        REQUIRE(warningAlerts[0].severity == AlertSeverity::Warning);
    }

    SECTION("Filter by type") {
        AlertFilter filter;
        filter.type = AlertType::HostDown;

        auto hostDownAlerts = alertsVm.getFilteredAlerts(filter, 100);
        REQUIRE(hostDownAlerts.size() == 1);
        REQUIRE(hostDownAlerts[0].type == AlertType::HostDown);
    }

    SECTION("Filter by type - recovery") {
        AlertFilter filter;
        filter.type = AlertType::HostRecovered;

        auto recoveryAlerts = alertsVm.getFilteredAlerts(filter, 100);
        REQUIRE(recoveryAlerts.size() == 1);
        REQUIRE(recoveryAlerts[0].type == AlertType::HostRecovered);
    }

    SECTION("Combined filters") {
        AlertFilter filter;
        filter.type = AlertType::HighLatency;
        filter.severity = AlertSeverity::Critical;

        auto filtered = alertsVm.getFilteredAlerts(filter, 100);
        REQUIRE(filtered.size() == 1);
        REQUIRE(filtered[0].type == AlertType::HighLatency);
        REQUIRE(filtered[0].severity == AlertSeverity::Critical);
    }

    SECTION("Limit parameter respected") {
        auto limited = alertsVm.getRecentAlerts(2);
        REQUIRE(limited.size() == 2);
    }

    SECTION("Alerts ordered by timestamp descending") {
        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 4);

        for (size_t i = 1; i < alerts.size(); ++i) {
            REQUIRE(alerts[i - 1].timestamp >= alerts[i].timestamp);
        }
    }
}

TEST_CASE("Alert workflow - clear alerts",
          "[Integration][Alert][Clear]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t hostId = hostRepo.insert(createTestHost("Clear Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 50;
    thresholds.consecutiveFailuresForDown = 3;
    alertsVm.setThresholds(thresholds);

    // Generate alerts
    alertsVm.processResult(hostId, "Clear Host", createSuccessfulPing(hostId, 100.0));
    alertsVm.processResult(hostId, "Clear Host", createSuccessfulPing(hostId, 150.0));

    SECTION("Clear removes all alerts") {
        REQUIRE(alertsVm.getRecentAlerts(100).size() == 2);
        REQUIRE(alertsVm.unacknowledgedCount() == 2);

        alertsVm.clearAlerts();

        REQUIRE(alertsVm.getRecentAlerts(100).empty());
        REQUIRE(alertsVm.unacknowledgedCount() == 0);

        // Verify database is also cleared
        auto dbAlerts = metricsRepo.getAlerts(100);
        REQUIRE(dbAlerts.empty());
    }
}

TEST_CASE("Alert workflow - threshold configuration",
          "[Integration][Alert][Thresholds]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t hostId = hostRepo.insert(createTestHost("Threshold Host", "192.168.1.1"));

    SECTION("Different threshold values affect alert generation") {
        // Strict thresholds
        AlertThresholds strictThresholds;
        strictThresholds.latencyWarningMs = 10;
        strictThresholds.latencyCriticalMs = 50;
        strictThresholds.consecutiveFailuresForDown = 1;
        alertsVm.setThresholds(strictThresholds);

        // 20ms should be warning with strict thresholds
        alertsVm.processResult(hostId, "Threshold Host", createSuccessfulPing(hostId, 20.0));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].severity == AlertSeverity::Warning);

        alertsVm.clearAlerts();

        // Relaxed thresholds
        AlertThresholds relaxedThresholds;
        relaxedThresholds.latencyWarningMs = 200;
        relaxedThresholds.latencyCriticalMs = 1000;
        relaxedThresholds.consecutiveFailuresForDown = 5;
        alertsVm.setThresholds(relaxedThresholds);

        // Same 20ms should not trigger alert with relaxed thresholds
        alertsVm.processResult(hostId, "Threshold Host", createSuccessfulPing(hostId, 20.0));

        auto relaxedAlerts = alertsVm.getRecentAlerts(100);
        REQUIRE(relaxedAlerts.empty());
    }

    SECTION("Threshold of 1 consecutive failure triggers immediately") {
        AlertThresholds thresholds;
        thresholds.consecutiveFailuresForDown = 1;
        alertsVm.setThresholds(thresholds);

        alertsVm.processResult(hostId, "Threshold Host", createFailedPing(hostId));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HostDown);
    }
}
