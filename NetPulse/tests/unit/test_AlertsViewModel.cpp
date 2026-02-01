#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"
#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "viewmodels/AlertsViewModel.hpp"

#include <QCoreApplication>
#include <chrono>
#include <filesystem>

using namespace netpulse::core;
using namespace netpulse::infra;
using namespace netpulse::viewmodels;

namespace {

class TestDatabase {
public:
    TestDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_alertsvm_test.db") {
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

PingResult createSuccessfulPing(int64_t hostId, double latencyMs = 50.0) {
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
// Construction and Threshold Tests
// =============================================================================

TEST_CASE("AlertsViewModel construction", "[AlertsViewModel][Construction]") {
    TestDatabase testDb;

    SECTION("Constructs with database only") {
        REQUIRE_NOTHROW(AlertsViewModel(testDb.get()));
    }

    SECTION("Constructs with database and null notification service") {
        REQUIRE_NOTHROW(AlertsViewModel(testDb.get(), nullptr));
    }
}

TEST_CASE("AlertsViewModel threshold management", "[AlertsViewModel][Thresholds]") {
    TestDatabase testDb;
    AlertsViewModel vm(testDb.get());

    SECTION("Default thresholds have sensible values") {
        auto thresholds = vm.getThresholds();
        REQUIRE(thresholds.latencyWarningMs > 0);
        REQUIRE(thresholds.latencyCriticalMs > thresholds.latencyWarningMs);
        REQUIRE(thresholds.consecutiveFailuresForDown > 0);
    }

    SECTION("Set and get thresholds") {
        AlertThresholds custom;
        custom.latencyWarningMs = 50;
        custom.latencyCriticalMs = 200;
        custom.packetLossWarningPercent = 10.0;
        custom.packetLossCriticalPercent = 30.0;
        custom.consecutiveFailuresForDown = 5;

        vm.setThresholds(custom);
        auto retrieved = vm.getThresholds();

        REQUIRE(retrieved.latencyWarningMs == 50);
        REQUIRE(retrieved.latencyCriticalMs == 200);
        REQUIRE(retrieved.packetLossWarningPercent == 10.0);
        REQUIRE(retrieved.packetLossCriticalPercent == 30.0);
        REQUIRE(retrieved.consecutiveFailuresForDown == 5);
    }
}

// =============================================================================
// Alert Processing Tests - Latency
// =============================================================================

TEST_CASE("AlertsViewModel latency alert processing", "[AlertsViewModel][ProcessResult][Latency]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    vm.setThresholds(thresholds);

    SECTION("No alert for normal latency") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 50.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }

    SECTION("Warning alert for high latency at threshold") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 100.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HighLatency);
        REQUIRE(alerts[0].severity == AlertSeverity::Warning);
        REQUIRE(alerts[0].hostId == hostId);
    }

    SECTION("Warning alert for high latency above warning threshold") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 250.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HighLatency);
        REQUIRE(alerts[0].severity == AlertSeverity::Warning);
    }

    SECTION("Critical alert for latency at critical threshold") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 500.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HighLatency);
        REQUIRE(alerts[0].severity == AlertSeverity::Critical);
    }

    SECTION("Critical alert for latency above critical threshold") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 1000.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HighLatency);
        REQUIRE(alerts[0].severity == AlertSeverity::Critical);
    }

    SECTION("Alert message contains latency value") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 600.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].message.find("600") != std::string::npos);
        REQUIRE(alerts[0].message.find("Test Host") != std::string::npos);
    }
}

// =============================================================================
// Alert Processing Tests - Host Down
// =============================================================================

TEST_CASE("AlertsViewModel host down alert processing", "[AlertsViewModel][ProcessResult][HostDown]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    vm.setThresholds(thresholds);

    SECTION("No alert for single failure") {
        vm.processResult(hostId, "Test Host", createFailedPing(hostId));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }

    SECTION("No alert for two consecutive failures") {
        vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        vm.processResult(hostId, "Test Host", createFailedPing(hostId));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }

    SECTION("HostDown alert after consecutive failures threshold") {
        for (int i = 0; i < 3; ++i) {
            vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        }

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HostDown);
        REQUIRE(alerts[0].severity == AlertSeverity::Critical);
        REQUIRE(alerts[0].hostId == hostId);
    }

    SECTION("Only one HostDown alert for continued failures") {
        for (int i = 0; i < 10; ++i) {
            vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        }

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HostDown);
    }

    SECTION("HostDown message contains failure count") {
        for (int i = 0; i < 3; ++i) {
            vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        }

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].message.find("3") != std::string::npos);
        REQUIRE(alerts[0].message.find("Test Host") != std::string::npos);
    }
}

// =============================================================================
// Alert Processing Tests - Host Recovery
// =============================================================================

TEST_CASE("AlertsViewModel host recovery alert processing", "[AlertsViewModel][ProcessResult][Recovery]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    vm.setThresholds(thresholds);

    SECTION("HostRecovered alert when host comes back online") {
        // First, make the host go down
        for (int i = 0; i < 3; ++i) {
            vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        }

        // Then host recovers
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 50.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 2);

        // Most recent alert should be recovery (alerts ordered by timestamp DESC)
        REQUIRE(alerts[0].type == AlertType::HostRecovered);
        REQUIRE(alerts[0].severity == AlertSeverity::Info);
        REQUIRE(alerts[0].hostId == hostId);

        REQUIRE(alerts[1].type == AlertType::HostDown);
    }

    SECTION("No recovery alert if host was never down") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 50.0));
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 60.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }

    SECTION("Failure counter resets after successful ping") {
        // Two failures (not enough for HostDown)
        vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        vm.processResult(hostId, "Test Host", createFailedPing(hostId));

        // Success resets counter
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 50.0));

        // Two more failures
        vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        vm.processResult(hostId, "Test Host", createFailedPing(hostId));

        // Should not trigger HostDown because counter was reset
        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }

    SECTION("Recovery message contains host name") {
        for (int i = 0; i < 3; ++i) {
            vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        }
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 50.0));

        auto alerts = vm.getRecentAlerts(100);
        auto recoveryAlert = alerts[0];
        REQUIRE(recoveryAlert.type == AlertType::HostRecovered);
        REQUIRE(recoveryAlert.message.find("Test Host") != std::string::npos);
    }
}

// =============================================================================
// Alert Processing Tests - Multiple Hosts
// =============================================================================

TEST_CASE("AlertsViewModel multiple host handling", "[AlertsViewModel][ProcessResult][MultiHost]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t host1Id = hostRepo.insert(createTestHost("Host 1", "192.168.1.1"));
    int64_t host2Id = hostRepo.insert(createTestHost("Host 2", "192.168.1.2"));

    AlertThresholds thresholds;
    thresholds.consecutiveFailuresForDown = 2;
    vm.setThresholds(thresholds);

    SECTION("Failure counters are independent per host") {
        vm.processResult(host1Id, "Host 1", createFailedPing(host1Id));
        vm.processResult(host2Id, "Host 2", createFailedPing(host2Id));
        vm.processResult(host1Id, "Host 1", createFailedPing(host1Id));

        // Host 1 should be down (2 failures), Host 2 only has 1 failure
        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].hostId == host1Id);
        REQUIRE(alerts[0].type == AlertType::HostDown);
    }

    SECTION("Both hosts can go down independently") {
        vm.processResult(host1Id, "Host 1", createFailedPing(host1Id));
        vm.processResult(host1Id, "Host 1", createFailedPing(host1Id));
        vm.processResult(host2Id, "Host 2", createFailedPing(host2Id));
        vm.processResult(host2Id, "Host 2", createFailedPing(host2Id));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 2);

        bool host1Down = false, host2Down = false;
        for (const auto& alert : alerts) {
            if (alert.hostId == host1Id && alert.type == AlertType::HostDown)
                host1Down = true;
            if (alert.hostId == host2Id && alert.type == AlertType::HostDown)
                host2Down = true;
        }
        REQUIRE(host1Down);
        REQUIRE(host2Down);
    }

    SECTION("Recovery is independent per host") {
        // Both hosts go down
        vm.processResult(host1Id, "Host 1", createFailedPing(host1Id));
        vm.processResult(host1Id, "Host 1", createFailedPing(host1Id));
        vm.processResult(host2Id, "Host 2", createFailedPing(host2Id));
        vm.processResult(host2Id, "Host 2", createFailedPing(host2Id));

        // Only host 1 recovers
        vm.processResult(host1Id, "Host 1", createSuccessfulPing(host1Id, 50.0));

        auto alerts = vm.getRecentAlerts(100);

        int recoveryCount = 0;
        for (const auto& alert : alerts) {
            if (alert.type == AlertType::HostRecovered) {
                recoveryCount++;
                REQUIRE(alert.hostId == host1Id);
            }
        }
        REQUIRE(recoveryCount == 1);
    }
}

// =============================================================================
// Subscription Tests
// =============================================================================

TEST_CASE("AlertsViewModel subscription system", "[AlertsViewModel][Subscription]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    vm.setThresholds(thresholds);

    SECTION("Subscriber receives alerts") {
        std::vector<Alert> receivedAlerts;
        vm.subscribe([&receivedAlerts](const Alert& alert) { receivedAlerts.push_back(alert); });

        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 600.0));

        REQUIRE(receivedAlerts.size() == 1);
        REQUIRE(receivedAlerts[0].type == AlertType::HighLatency);
    }

    SECTION("Multiple subscribers receive same alert") {
        int count1 = 0, count2 = 0;
        vm.subscribe([&count1](const Alert&) { count1++; });
        vm.subscribe([&count2](const Alert&) { count2++; });

        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 600.0));

        REQUIRE(count1 == 1);
        REQUIRE(count2 == 1);
    }

    SECTION("unsubscribeAll removes all subscribers") {
        int count = 0;
        vm.subscribe([&count](const Alert&) { count++; });
        vm.subscribe([&count](const Alert&) { count++; });

        vm.unsubscribeAll();
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 600.0));

        REQUIRE(count == 0);
    }
}

// =============================================================================
// Alert Retrieval Tests
// =============================================================================

TEST_CASE("AlertsViewModel alert retrieval", "[AlertsViewModel][Retrieval]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 50;
    thresholds.latencyCriticalMs = 100;
    thresholds.consecutiveFailuresForDown = 3;
    vm.setThresholds(thresholds);

    SECTION("getRecentAlerts returns empty when no alerts") {
        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.empty());
    }

    SECTION("getRecentAlerts respects limit") {
        for (int i = 0; i < 10; ++i) {
            vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 150.0));
        }

        auto limited = vm.getRecentAlerts(5);
        REQUIRE(limited.size() == 5);
    }

    SECTION("getFilteredAlerts filters by severity") {
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 75.0));   // Warning
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 150.0));  // Critical

        AlertFilter filter;
        filter.severity = AlertSeverity::Critical;

        auto criticalAlerts = vm.getFilteredAlerts(filter, 100);
        REQUIRE(criticalAlerts.size() == 1);
        REQUIRE(criticalAlerts[0].severity == AlertSeverity::Critical);
    }

    SECTION("getFilteredAlerts filters by type") {
        // Generate HighLatency alert
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 150.0));

        // Generate HostDown alert
        for (int i = 0; i < 3; ++i) {
            vm.processResult(hostId, "Test Host", createFailedPing(hostId));
        }

        AlertFilter filter;
        filter.type = AlertType::HostDown;

        auto hostDownAlerts = vm.getFilteredAlerts(filter, 100);
        REQUIRE(hostDownAlerts.size() == 1);
        REQUIRE(hostDownAlerts[0].type == AlertType::HostDown);
    }
}

// =============================================================================
// Alert Acknowledgment Tests
// =============================================================================

TEST_CASE("AlertsViewModel alert acknowledgment", "[AlertsViewModel][Acknowledgment]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 50;
    thresholds.latencyCriticalMs = 100;
    thresholds.consecutiveFailuresForDown = 3;
    vm.setThresholds(thresholds);

    // Generate some alerts
    vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 150.0));
    vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 75.0));

    SECTION("unacknowledgedCount returns correct count") {
        REQUIRE(vm.unacknowledgedCount() == 2);
    }

    SECTION("acknowledgeAlert marks single alert as acknowledged") {
        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 2);

        vm.acknowledgeAlert(alerts[0].id);

        REQUIRE(vm.unacknowledgedCount() == 1);
    }

    SECTION("acknowledgeAll marks all alerts as acknowledged") {
        REQUIRE(vm.unacknowledgedCount() == 2);

        vm.acknowledgeAll();

        REQUIRE(vm.unacknowledgedCount() == 0);
    }

    SECTION("getFilteredAlerts can filter by acknowledged status") {
        auto alerts = vm.getRecentAlerts(100);
        vm.acknowledgeAlert(alerts[0].id);

        AlertFilter filterAck;
        filterAck.acknowledged = true;
        auto acknowledged = vm.getFilteredAlerts(filterAck, 100);
        REQUIRE(acknowledged.size() == 1);

        AlertFilter filterUnack;
        filterUnack.acknowledged = false;
        auto unacknowledged = vm.getFilteredAlerts(filterUnack, 100);
        REQUIRE(unacknowledged.size() == 1);
    }
}

// =============================================================================
// Clear Alerts Tests
// =============================================================================

TEST_CASE("AlertsViewModel clear alerts", "[AlertsViewModel][Clear]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 50;
    thresholds.consecutiveFailuresForDown = 3;
    vm.setThresholds(thresholds);

    // Generate some alerts
    vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 100.0));
    vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 150.0));

    SECTION("clearAlerts removes all alerts") {
        REQUIRE(vm.getRecentAlerts(100).size() == 2);

        vm.clearAlerts();

        REQUIRE(vm.getRecentAlerts(100).empty());
        REQUIRE(vm.unacknowledgedCount() == 0);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("AlertsViewModel edge cases", "[AlertsViewModel][EdgeCases]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    AlertsViewModel vm(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    SECTION("Handle zero latency threshold") {
        AlertThresholds thresholds;
        thresholds.latencyWarningMs = 0;
        thresholds.latencyCriticalMs = 0;
        thresholds.consecutiveFailuresForDown = 3;
        vm.setThresholds(thresholds);

        // Any successful ping should trigger critical alert
        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 1.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].severity == AlertSeverity::Critical);
    }

    SECTION("Handle very high latency") {
        AlertThresholds thresholds;
        thresholds.latencyWarningMs = 100;
        thresholds.latencyCriticalMs = 500;
        thresholds.consecutiveFailuresForDown = 3;
        vm.setThresholds(thresholds);

        vm.processResult(hostId, "Test Host", createSuccessfulPing(hostId, 10000.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].severity == AlertSeverity::Critical);
    }

    SECTION("Acknowledge non-existent alert ID does not throw") {
        REQUIRE_NOTHROW(vm.acknowledgeAlert(99999));
    }

    SECTION("Host name is stored correctly") {
        AlertThresholds thresholds;
        thresholds.latencyWarningMs = 50;
        thresholds.consecutiveFailuresForDown = 3;
        vm.setThresholds(thresholds);

        vm.processResult(hostId, "Custom Name", createSuccessfulPing(hostId, 100.0));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts[0].message.find("Custom Name") != std::string::npos);
    }

    SECTION("Consecutive failures threshold of 1 triggers immediately") {
        AlertThresholds thresholds;
        thresholds.consecutiveFailuresForDown = 1;
        vm.setThresholds(thresholds);

        vm.processResult(hostId, "Test Host", createFailedPing(hostId));

        auto alerts = vm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].type == AlertType::HostDown);
    }
}
