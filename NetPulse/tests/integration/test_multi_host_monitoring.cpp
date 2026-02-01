#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"
#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "infrastructure/network/AsioContext.hpp"
#include "infrastructure/network/PingService.hpp"
#include "viewmodels/AlertsViewModel.hpp"
#include "viewmodels/DashboardViewModel.hpp"

#include <QCoreApplication>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>

using namespace netpulse::core;
using namespace netpulse::infra;
using namespace netpulse::viewmodels;

namespace {

class IntegrationTestDatabase {
public:
    IntegrationTestDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_multihost_integration_test.db") {
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

Host createTestHost(const std::string& name, const std::string& address, int intervalSeconds = 1) {
    Host host;
    host.name = name;
    host.address = address;
    host.pingIntervalSeconds = intervalSeconds;
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
// Multi-Host Monitoring Integration Tests
// =============================================================================

TEST_CASE("Multi-host monitoring - independent failure tracking",
          "[Integration][MultiHost][Failures]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t host1Id = hostRepo.insert(createTestHost("Host 1", "192.168.1.1"));
    int64_t host2Id = hostRepo.insert(createTestHost("Host 2", "192.168.1.2"));
    int64_t host3Id = hostRepo.insert(createTestHost("Host 3", "192.168.1.3"));

    AlertThresholds thresholds;
    thresholds.consecutiveFailuresForDown = 3;
    alertsVm.setThresholds(thresholds);

    SECTION("Each host tracks failures independently") {
        // Host 1: 2 failures
        alertsVm.processResult(host1Id, "Host 1", createFailedPing(host1Id));
        alertsVm.processResult(host1Id, "Host 1", createFailedPing(host1Id));

        // Host 2: 3 failures (should trigger alert)
        for (int i = 0; i < 3; ++i) {
            alertsVm.processResult(host2Id, "Host 2", createFailedPing(host2Id));
        }

        // Host 3: 1 failure
        alertsVm.processResult(host3Id, "Host 3", createFailedPing(host3Id));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].hostId == host2Id);
        REQUIRE(alerts[0].type == AlertType::HostDown);
    }

    SECTION("Success on one host doesn't reset other hosts' counters") {
        // Host 1: 2 failures
        alertsVm.processResult(host1Id, "Host 1", createFailedPing(host1Id));
        alertsVm.processResult(host1Id, "Host 1", createFailedPing(host1Id));

        // Host 2: 2 failures
        alertsVm.processResult(host2Id, "Host 2", createFailedPing(host2Id));
        alertsVm.processResult(host2Id, "Host 2", createFailedPing(host2Id));

        // Host 1: Success (resets Host 1 counter, not Host 2)
        alertsVm.processResult(host1Id, "Host 1", createSuccessfulPing(host1Id, 50.0));

        // Host 2: One more failure (should now trigger alert)
        alertsVm.processResult(host2Id, "Host 2", createFailedPing(host2Id));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].hostId == host2Id);
        REQUIRE(alerts[0].type == AlertType::HostDown);
    }
}

TEST_CASE("Multi-host monitoring - multiple hosts can be down simultaneously",
          "[Integration][MultiHost][Down]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t host1Id = hostRepo.insert(createTestHost("Host 1", "192.168.1.1"));
    int64_t host2Id = hostRepo.insert(createTestHost("Host 2", "192.168.1.2"));
    int64_t host3Id = hostRepo.insert(createTestHost("Host 3", "192.168.1.3"));

    AlertThresholds thresholds;
    thresholds.consecutiveFailuresForDown = 2;
    alertsVm.setThresholds(thresholds);

    SECTION("All hosts can go down independently") {
        // All three hosts go down
        for (int64_t hostId : {host1Id, host2Id, host3Id}) {
            alertsVm.processResult(hostId, "Host " + std::to_string(hostId), createFailedPing(hostId));
            alertsVm.processResult(hostId, "Host " + std::to_string(hostId), createFailedPing(hostId));
        }

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 3);

        std::set<int64_t> downHostIds;
        for (const auto& alert : alerts) {
            REQUIRE(alert.type == AlertType::HostDown);
            downHostIds.insert(alert.hostId);
        }

        REQUIRE(downHostIds.count(host1Id) == 1);
        REQUIRE(downHostIds.count(host2Id) == 1);
        REQUIRE(downHostIds.count(host3Id) == 1);
    }

    SECTION("Hosts can recover independently") {
        // All hosts go down
        for (int64_t hostId : {host1Id, host2Id, host3Id}) {
            alertsVm.processResult(hostId, "Host " + std::to_string(hostId), createFailedPing(hostId));
            alertsVm.processResult(hostId, "Host " + std::to_string(hostId), createFailedPing(hostId));
        }

        // Only Host 2 recovers
        alertsVm.processResult(host2Id, "Host 2", createSuccessfulPing(host2Id, 50.0));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 4); // 3 down + 1 recovery

        int recoveryCount = 0;
        for (const auto& alert : alerts) {
            if (alert.type == AlertType::HostRecovered) {
                recoveryCount++;
                REQUIRE(alert.hostId == host2Id);
            }
        }
        REQUIRE(recoveryCount == 1);
    }
}

TEST_CASE("Multi-host monitoring - independent latency alerts",
          "[Integration][MultiHost][Latency]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t host1Id = hostRepo.insert(createTestHost("Host 1", "192.168.1.1"));
    int64_t host2Id = hostRepo.insert(createTestHost("Host 2", "192.168.1.2"));
    int64_t host3Id = hostRepo.insert(createTestHost("Host 3", "192.168.1.3"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 3;
    alertsVm.setThresholds(thresholds);

    SECTION("Different latency levels trigger appropriate alerts per host") {
        // Host 1: Normal latency (no alert)
        alertsVm.processResult(host1Id, "Host 1", createSuccessfulPing(host1Id, 50.0));

        // Host 2: Warning latency
        alertsVm.processResult(host2Id, "Host 2", createSuccessfulPing(host2Id, 150.0));

        // Host 3: Critical latency
        alertsVm.processResult(host3Id, "Host 3", createSuccessfulPing(host3Id, 600.0));

        auto alerts = alertsVm.getRecentAlerts(100);
        REQUIRE(alerts.size() == 2);

        std::map<int64_t, AlertSeverity> hostSeverities;
        for (const auto& alert : alerts) {
            hostSeverities[alert.hostId] = alert.severity;
        }

        REQUIRE(hostSeverities.find(host1Id) == hostSeverities.end()); // No alert
        REQUIRE(hostSeverities[host2Id] == AlertSeverity::Warning);
        REQUIRE(hostSeverities[host3Id] == AlertSeverity::Critical);
    }
}

TEST_CASE("Multi-host monitoring - data isolation in database",
          "[Integration][MultiHost][Database]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    int64_t host1Id = hostRepo.insert(createTestHost("Host 1", "192.168.1.1"));
    int64_t host2Id = hostRepo.insert(createTestHost("Host 2", "192.168.1.2"));
    int64_t host3Id = hostRepo.insert(createTestHost("Host 3", "192.168.1.3"));

    SECTION("Ping results are stored per host") {
        // Insert ping results for each host
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createSuccessfulPing(host1Id, 10.0 + i));
        }
        for (int i = 0; i < 3; ++i) {
            metricsRepo.insertPingResult(createSuccessfulPing(host2Id, 20.0 + i));
        }
        for (int i = 0; i < 7; ++i) {
            metricsRepo.insertPingResult(createSuccessfulPing(host3Id, 30.0 + i));
        }

        auto host1Results = metricsRepo.getPingResults(host1Id, 100);
        auto host2Results = metricsRepo.getPingResults(host2Id, 100);
        auto host3Results = metricsRepo.getPingResults(host3Id, 100);

        REQUIRE(host1Results.size() == 5);
        REQUIRE(host2Results.size() == 3);
        REQUIRE(host3Results.size() == 7);

        for (const auto& result : host1Results) {
            REQUIRE(result.hostId == host1Id);
        }
        for (const auto& result : host2Results) {
            REQUIRE(result.hostId == host2Id);
        }
        for (const auto& result : host3Results) {
            REQUIRE(result.hostId == host3Id);
        }
    }

    SECTION("Statistics are calculated per host") {
        // Insert varied results for each host
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertPingResult(createSuccessfulPing(host1Id, 10.0));
        }

        for (int i = 0; i < 10; ++i) {
            if (i % 2 == 0) {
                metricsRepo.insertPingResult(createSuccessfulPing(host2Id, 100.0));
            } else {
                metricsRepo.insertPingResult(createFailedPing(host2Id));
            }
        }

        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertPingResult(createFailedPing(host3Id));
        }

        auto stats1 = metricsRepo.getStatistics(host1Id);
        auto stats2 = metricsRepo.getStatistics(host2Id);
        auto stats3 = metricsRepo.getStatistics(host3Id);

        REQUIRE(stats1.totalPings == 10);
        REQUIRE(stats1.successfulPings == 10);
        REQUIRE(stats1.packetLossPercent == 0.0);

        REQUIRE(stats2.totalPings == 10);
        REQUIRE(stats2.successfulPings == 5);
        REQUIRE(stats2.packetLossPercent == 50.0);

        REQUIRE(stats3.totalPings == 10);
        REQUIRE(stats3.successfulPings == 0);
        REQUIRE(stats3.packetLossPercent == 100.0);
    }
}

TEST_CASE("Multi-host monitoring - concurrent ping service monitoring",
          "[Integration][MultiHost][PingService]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);

    Host host1 = createTestHost("Concurrent 1", "192.168.80.1", 1);
    Host host2 = createTestHost("Concurrent 2", "192.168.80.2", 1);
    Host host3 = createTestHost("Concurrent 3", "192.168.80.3", 1);

    int64_t id1 = hostRepo.insert(host1);
    int64_t id2 = hostRepo.insert(host2);
    int64_t id3 = hostRepo.insert(host3);

    host1.id = id1;
    host2.id = id2;
    host3.id = id3;

    SECTION("All hosts receive callbacks independently") {
        std::mutex mtx;
        std::condition_variable cv;
        std::map<int64_t, int> resultCounts;
        resultCounts[id1] = 0;
        resultCounts[id2] = 0;
        resultCounts[id3] = 0;

        pingService->startMonitoring(host1, [&](const PingResult&) {
            std::lock_guard<std::mutex> lock(mtx);
            resultCounts[id1]++;
            cv.notify_one();
        });

        pingService->startMonitoring(host2, [&](const PingResult&) {
            std::lock_guard<std::mutex> lock(mtx);
            resultCounts[id2]++;
            cv.notify_one();
        });

        pingService->startMonitoring(host3, [&](const PingResult&) {
            std::lock_guard<std::mutex> lock(mtx);
            resultCounts[id3]++;
            cv.notify_one();
        });

        REQUIRE(pingService->isMonitoring(id1));
        REQUIRE(pingService->isMonitoring(id2));
        REQUIRE(pingService->isMonitoring(id3));

        // Wait for at least one result from each host
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(15), [&] {
                return resultCounts[id1] >= 1 && resultCounts[id2] >= 1 && resultCounts[id3] >= 1;
            });
        }

        pingService->stopAllMonitoring();

        REQUIRE(resultCounts[id1] >= 1);
        REQUIRE(resultCounts[id2] >= 1);
        REQUIRE(resultCounts[id3] >= 1);
    }

    context.stop();
}

TEST_CASE("Multi-host monitoring - mixed alert scenarios",
          "[Integration][MultiHost][Mixed]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    AlertsViewModel alertsVm(db);

    int64_t host1Id = hostRepo.insert(createTestHost("Mixed 1", "192.168.1.1"));
    int64_t host2Id = hostRepo.insert(createTestHost("Mixed 2", "192.168.1.2"));
    int64_t host3Id = hostRepo.insert(createTestHost("Mixed 3", "192.168.1.3"));

    AlertThresholds thresholds;
    thresholds.latencyWarningMs = 100;
    thresholds.latencyCriticalMs = 500;
    thresholds.consecutiveFailuresForDown = 2;
    alertsVm.setThresholds(thresholds);

    SECTION("Different hosts can have different alert types simultaneously") {
        // Host 1: High latency
        alertsVm.processResult(host1Id, "Mixed 1", createSuccessfulPing(host1Id, 200.0));

        // Host 2: Down
        alertsVm.processResult(host2Id, "Mixed 2", createFailedPing(host2Id));
        alertsVm.processResult(host2Id, "Mixed 2", createFailedPing(host2Id));

        // Host 3: Recovery (needs to be down first)
        alertsVm.processResult(host3Id, "Mixed 3", createFailedPing(host3Id));
        alertsVm.processResult(host3Id, "Mixed 3", createFailedPing(host3Id));
        alertsVm.processResult(host3Id, "Mixed 3", createSuccessfulPing(host3Id, 50.0));

        auto alerts = alertsVm.getRecentAlerts(100);

        std::map<int64_t, std::set<AlertType>> hostAlertTypes;
        for (const auto& alert : alerts) {
            hostAlertTypes[alert.hostId].insert(alert.type);
        }

        REQUIRE(hostAlertTypes[host1Id].count(AlertType::HighLatency) == 1);
        REQUIRE(hostAlertTypes[host2Id].count(AlertType::HostDown) == 1);
        REQUIRE(hostAlertTypes[host3Id].count(AlertType::HostDown) == 1);
        REQUIRE(hostAlertTypes[host3Id].count(AlertType::HostRecovered) == 1);
    }

    SECTION("Interleaved host events produce correct alerts") {
        // Interleave events from different hosts
        alertsVm.processResult(host1Id, "Mixed 1", createFailedPing(host1Id));        // H1: 1 fail
        alertsVm.processResult(host2Id, "Mixed 2", createSuccessfulPing(host2Id, 200.0)); // H2: warning
        alertsVm.processResult(host3Id, "Mixed 3", createSuccessfulPing(host3Id, 50.0));  // H3: ok
        alertsVm.processResult(host1Id, "Mixed 1", createFailedPing(host1Id));        // H1: 2 fails -> down
        alertsVm.processResult(host2Id, "Mixed 2", createSuccessfulPing(host2Id, 600.0)); // H2: critical
        alertsVm.processResult(host3Id, "Mixed 3", createSuccessfulPing(host3Id, 50.0));  // H3: ok

        auto alerts = alertsVm.getRecentAlerts(100);

        // Should have: H1 down, H2 warning, H2 critical
        int host1Alerts = 0, host2Alerts = 0, host3Alerts = 0;
        for (const auto& alert : alerts) {
            if (alert.hostId == host1Id) host1Alerts++;
            else if (alert.hostId == host2Id) host2Alerts++;
            else if (alert.hostId == host3Id) host3Alerts++;
        }

        REQUIRE(host1Alerts == 1); // HostDown
        REQUIRE(host2Alerts == 2); // Warning + Critical
        REQUIRE(host3Alerts == 0); // No alerts
    }
}

TEST_CASE("Multi-host monitoring - DashboardViewModel with multiple hosts",
          "[Integration][MultiHost][Dashboard]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);

    SECTION("DashboardViewModel reports correct host counts") {
        Host host1 = createTestHost("Dashboard 1", "192.168.100.1");
        Host host2 = createTestHost("Dashboard 2", "192.168.100.2");
        Host host3 = createTestHost("Dashboard 3", "192.168.100.3");
        Host host4 = createTestHost("Dashboard 4", "192.168.100.4");
        Host host5 = createTestHost("Dashboard 5", "192.168.100.5");

        hostRepo.insert(host1);
        hostRepo.insert(host2);
        hostRepo.insert(host3);
        hostRepo.insert(host4);
        hostRepo.insert(host5);

        DashboardViewModel dashboard(db, pingService);

        REQUIRE(dashboard.hostCount() == 5);
        REQUIRE(dashboard.getHosts().size() == 5);
    }

    context.stop();
}
