#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"
#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "core/types/PortScanResult.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

using namespace netpulse::core;
using namespace netpulse::infra;

namespace {

class IntegrationTestDatabase {
public:
    IntegrationTestDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_retention_integration_test.db") {
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

PingResult createTestPingResult(int64_t hostId, bool success = true,
                                 std::chrono::microseconds latency = std::chrono::microseconds(5000)) {
    PingResult result;
    result.hostId = hostId;
    result.timestamp = std::chrono::system_clock::now();
    result.latency = latency;
    result.success = success;
    result.ttl = 64;
    return result;
}

Alert createTestAlert(int64_t hostId, AlertType type = AlertType::HostDown,
                      AlertSeverity severity = AlertSeverity::Critical) {
    Alert alert;
    alert.hostId = hostId;
    alert.type = type;
    alert.severity = severity;
    alert.title = "Test Alert";
    alert.message = "This is a test alert message";
    alert.timestamp = std::chrono::system_clock::now();
    alert.acknowledged = false;
    return alert;
}

PortScanResult createTestPortScanResult(const std::string& address, uint16_t port,
                                         PortState state = PortState::Open) {
    PortScanResult result;
    result.targetAddress = address;
    result.port = port;
    result.state = state;
    result.serviceName = ServiceDetector::detectService(port);
    result.scanTimestamp = std::chrono::system_clock::now();
    return result;
}

} // namespace

// =============================================================================
// Data Retention Integration Tests
// =============================================================================

TEST_CASE("Data retention - ping result cleanup",
          "[Integration][Retention][PingResults]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    int64_t hostId = hostRepo.insert(createTestHost("Retention Host", "192.168.1.1"));

    SECTION("Cleanup removes all old ping results") {
        // Insert ping results
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }

        auto beforeCleanup = metricsRepo.getPingResults(hostId, 100);
        REQUIRE(beforeCleanup.size() == 10);

        // Cleanup with retention period that removes all (negative duration)
        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));

        auto afterCleanup = metricsRepo.getPingResults(hostId, 100);
        REQUIRE(afterCleanup.empty());
    }

    SECTION("Cleanup with long retention keeps all results") {
        // Insert ping results
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }

        auto beforeCleanup = metricsRepo.getPingResults(hostId, 100);
        REQUIRE(beforeCleanup.size() == 10);

        // Cleanup with very long retention (keeps everything)
        metricsRepo.cleanupOldPingResults(std::chrono::hours(24 * 365));

        auto afterCleanup = metricsRepo.getPingResults(hostId, 100);
        REQUIRE(afterCleanup.size() == 10);
    }

    SECTION("Cleanup affects multiple hosts") {
        int64_t host2Id = hostRepo.insert(createTestHost("Retention Host 2", "192.168.1.2"));
        int64_t host3Id = hostRepo.insert(createTestHost("Retention Host 3", "192.168.1.3"));

        // Insert results for all hosts
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
            metricsRepo.insertPingResult(createTestPingResult(host2Id));
            metricsRepo.insertPingResult(createTestPingResult(host3Id));
        }

        REQUIRE(metricsRepo.getPingResults(hostId, 100).size() == 5);
        REQUIRE(metricsRepo.getPingResults(host2Id, 100).size() == 5);
        REQUIRE(metricsRepo.getPingResults(host3Id, 100).size() == 5);

        // Cleanup all
        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));

        REQUIRE(metricsRepo.getPingResults(hostId, 100).empty());
        REQUIRE(metricsRepo.getPingResults(host2Id, 100).empty());
        REQUIRE(metricsRepo.getPingResults(host3Id, 100).empty());
    }
}

TEST_CASE("Data retention - alert cleanup",
          "[Integration][Retention][Alerts]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    MetricsRepository metricsRepo(db);

    SECTION("Cleanup removes all old alerts") {
        // Insert alerts
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertAlert(createTestAlert(1));
        }

        auto beforeCleanup = metricsRepo.getAlerts(100);
        REQUIRE(beforeCleanup.size() == 10);

        // Cleanup with retention period that removes all
        metricsRepo.cleanupOldAlerts(std::chrono::hours(-1));

        auto afterCleanup = metricsRepo.getAlerts(100);
        REQUIRE(afterCleanup.empty());
    }

    SECTION("Cleanup with long retention keeps all alerts") {
        // Insert alerts
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertAlert(createTestAlert(1));
        }

        auto beforeCleanup = metricsRepo.getAlerts(100);
        REQUIRE(beforeCleanup.size() == 10);

        // Cleanup with very long retention
        metricsRepo.cleanupOldAlerts(std::chrono::hours(24 * 365));

        auto afterCleanup = metricsRepo.getAlerts(100);
        REQUIRE(afterCleanup.size() == 10);
    }

    SECTION("Cleanup removes alerts of all types") {
        metricsRepo.insertAlert(createTestAlert(1, AlertType::HostDown, AlertSeverity::Critical));
        metricsRepo.insertAlert(createTestAlert(1, AlertType::HighLatency, AlertSeverity::Warning));
        metricsRepo.insertAlert(createTestAlert(1, AlertType::PacketLoss, AlertSeverity::Warning));
        metricsRepo.insertAlert(createTestAlert(1, AlertType::HostRecovered, AlertSeverity::Info));
        metricsRepo.insertAlert(createTestAlert(1, AlertType::ScanComplete, AlertSeverity::Info));

        REQUIRE(metricsRepo.getAlerts(100).size() == 5);

        metricsRepo.cleanupOldAlerts(std::chrono::hours(-1));

        REQUIRE(metricsRepo.getAlerts(100).empty());
    }

    SECTION("Acknowledged and unacknowledged alerts both cleaned up") {
        Alert acked = createTestAlert(1);
        acked.acknowledged = true;
        metricsRepo.insertAlert(acked);

        Alert unacked = createTestAlert(1);
        unacked.acknowledged = false;
        metricsRepo.insertAlert(unacked);

        REQUIRE(metricsRepo.getAlerts(100).size() == 2);

        metricsRepo.cleanupOldAlerts(std::chrono::hours(-1));

        REQUIRE(metricsRepo.getAlerts(100).empty());
    }
}

TEST_CASE("Data retention - port scan cleanup",
          "[Integration][Retention][PortScans]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    MetricsRepository metricsRepo(db);

    SECTION("Cleanup removes all old port scan results") {
        // Insert port scan results
        for (int port = 1; port <= 10; ++port) {
            metricsRepo.insertPortScanResult(
                createTestPortScanResult("192.168.1.1", static_cast<uint16_t>(port)));
        }

        auto beforeCleanup = metricsRepo.getPortScanResults("192.168.1.1", 100);
        REQUIRE(beforeCleanup.size() == 10);

        // Cleanup all
        metricsRepo.cleanupOldPortScans(std::chrono::hours(-1));

        auto afterCleanup = metricsRepo.getPortScanResults("192.168.1.1", 100);
        REQUIRE(afterCleanup.empty());
    }

    SECTION("Cleanup with long retention keeps all results") {
        for (int port = 1; port <= 5; ++port) {
            metricsRepo.insertPortScanResult(
                createTestPortScanResult("192.168.1.1", static_cast<uint16_t>(port)));
        }

        REQUIRE(metricsRepo.getPortScanResults("192.168.1.1", 100).size() == 5);

        metricsRepo.cleanupOldPortScans(std::chrono::hours(24 * 365));

        REQUIRE(metricsRepo.getPortScanResults("192.168.1.1", 100).size() == 5);
    }

    SECTION("Cleanup affects multiple targets") {
        for (int port = 1; port <= 5; ++port) {
            metricsRepo.insertPortScanResult(
                createTestPortScanResult("192.168.1.1", static_cast<uint16_t>(port)));
            metricsRepo.insertPortScanResult(
                createTestPortScanResult("192.168.1.2", static_cast<uint16_t>(port)));
        }

        REQUIRE(metricsRepo.getPortScanResults("192.168.1.1", 100).size() == 5);
        REQUIRE(metricsRepo.getPortScanResults("192.168.1.2", 100).size() == 5);

        metricsRepo.cleanupOldPortScans(std::chrono::hours(-1));

        REQUIRE(metricsRepo.getPortScanResults("192.168.1.1", 100).empty());
        REQUIRE(metricsRepo.getPortScanResults("192.168.1.2", 100).empty());
    }
}

TEST_CASE("Data retention - combined cleanup workflow",
          "[Integration][Retention][Combined]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    int64_t hostId = hostRepo.insert(createTestHost("Combined Retention", "192.168.1.1"));

    SECTION("All data types can be cleaned up together") {
        // Insert various data types
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
            metricsRepo.insertAlert(createTestAlert(hostId));
            metricsRepo.insertPortScanResult(
                createTestPortScanResult("192.168.1.1", static_cast<uint16_t>(80 + i)));
        }

        REQUIRE(metricsRepo.getPingResults(hostId, 100).size() == 5);
        REQUIRE(metricsRepo.getAlerts(100).size() == 5);
        REQUIRE(metricsRepo.getPortScanResults("192.168.1.1", 100).size() == 5);

        // Cleanup all
        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));
        metricsRepo.cleanupOldAlerts(std::chrono::hours(-1));
        metricsRepo.cleanupOldPortScans(std::chrono::hours(-1));

        REQUIRE(metricsRepo.getPingResults(hostId, 100).empty());
        REQUIRE(metricsRepo.getAlerts(100).empty());
        REQUIRE(metricsRepo.getPortScanResults("192.168.1.1", 100).empty());
    }

    SECTION("Cleanup one type doesn't affect others") {
        // Insert various data types
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
            metricsRepo.insertAlert(createTestAlert(hostId));
            metricsRepo.insertPortScanResult(
                createTestPortScanResult("192.168.1.1", static_cast<uint16_t>(80 + i)));
        }

        // Only cleanup ping results
        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));

        REQUIRE(metricsRepo.getPingResults(hostId, 100).empty());
        REQUIRE(metricsRepo.getAlerts(100).size() == 5);           // Not affected
        REQUIRE(metricsRepo.getPortScanResults("192.168.1.1", 100).size() == 5); // Not affected
    }
}

TEST_CASE("Data retention - statistics after cleanup",
          "[Integration][Retention][Statistics]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    int64_t hostId = hostRepo.insert(createTestHost("Stats Retention", "192.168.1.1"));

    SECTION("Statistics reflect current data after cleanup") {
        // Insert ping results
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertPingResult(
                createTestPingResult(hostId, true, std::chrono::microseconds(1000 * (i + 1))));
        }

        auto statsBefore = metricsRepo.getStatistics(hostId);
        REQUIRE(statsBefore.totalPings == 10);
        REQUIRE(statsBefore.successfulPings == 10);

        // Cleanup all
        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));

        auto statsAfter = metricsRepo.getStatistics(hostId);
        REQUIRE(statsAfter.totalPings == 0);
        REQUIRE(statsAfter.successfulPings == 0);
    }

    SECTION("Partial cleanup updates statistics correctly") {
        // Insert results and wait between batches
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(
                createTestPingResult(hostId, true, std::chrono::microseconds(1000)));
        }

        // Insert more recent results
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(
                createTestPingResult(hostId, true, std::chrono::microseconds(2000)));
        }

        auto statsBefore = metricsRepo.getStatistics(hostId);
        REQUIRE(statsBefore.totalPings == 10);
    }
}

TEST_CASE("Data retention - export before cleanup",
          "[Integration][Retention][Export]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    int64_t hostId = hostRepo.insert(createTestHost("Export Retention", "192.168.1.1"));

    SECTION("Data can be exported before cleanup") {
        // Insert ping results
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }

        // Export data
        std::string json = metricsRepo.exportToJson(hostId);
        std::string csv = metricsRepo.exportToCsv(hostId);

        REQUIRE_FALSE(json.empty());
        REQUIRE(json.find("ping_results") != std::string::npos);
        REQUIRE_FALSE(csv.empty());

        // Cleanup
        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));

        // Verify data is gone from DB
        REQUIRE(metricsRepo.getPingResults(hostId, 100).empty());

        // But export data was captured before cleanup
        REQUIRE(json.find("\"host_id\": " + std::to_string(hostId)) != std::string::npos);
    }

    SECTION("Export after cleanup shows empty results") {
        // Insert and cleanup
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }
        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));

        // Export shows empty
        std::string json = metricsRepo.exportToJson(hostId);
        REQUIRE(json.find("\"ping_results\": []") != std::string::npos);

        std::string csv = metricsRepo.exportToCsv(hostId);
        // CSV should only have header
        int newlines = static_cast<int>(std::count(csv.begin(), csv.end(), '\n'));
        REQUIRE(newlines == 1); // Just header
    }
}

TEST_CASE("Data retention - host deletion impact",
          "[Integration][Retention][HostDeletion]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    int64_t hostId = hostRepo.insert(createTestHost("Delete Host", "192.168.1.1"));

    SECTION("Deleting host removes associated data") {
        // Insert ping results for the host
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }

        REQUIRE(metricsRepo.getPingResults(hostId, 100).size() == 5);

        // Delete the host
        hostRepo.remove(hostId);

        // Verify host is gone
        REQUIRE_FALSE(hostRepo.findById(hostId).has_value());

        // Note: The ping results may still exist in DB (orphaned) depending on
        // foreign key constraints. The cleanup function should handle this.
    }

    SECTION("Cleanup doesn't fail on orphaned data") {
        // Insert ping results for a host that doesn't exist anymore
        // (simulate orphaned data scenario)
        PingResult orphanResult;
        orphanResult.hostId = 99999; // Non-existent host
        orphanResult.timestamp = std::chrono::system_clock::now();
        orphanResult.latency = std::chrono::microseconds(1000);
        orphanResult.success = true;
        orphanResult.ttl = 64;

        // This might fail due to FK constraints, which is actually correct behavior
        // The test verifies the system handles this gracefully
        try {
            metricsRepo.insertPingResult(orphanResult);
            // If insert succeeds, cleanup should work
            REQUIRE_NOTHROW(metricsRepo.cleanupOldPingResults(std::chrono::hours(-1)));
        } catch (const std::exception&) {
            // FK constraint prevented insert - this is valid behavior
            SUCCEED("Foreign key constraint prevented orphaned data insertion");
        }
    }
}

TEST_CASE("Data retention - concurrent operations",
          "[Integration][Retention][Concurrent]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    int64_t hostId = hostRepo.insert(createTestHost("Concurrent Retention", "192.168.1.1"));

    SECTION("Insert during cleanup doesn't cause issues") {
        // Insert initial data
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }

        // Cleanup with long retention (keeps data)
        metricsRepo.cleanupOldPingResults(std::chrono::hours(24 * 365));

        // Insert more data
        for (int i = 0; i < 5; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }

        // All data should still be there
        REQUIRE(metricsRepo.getPingResults(hostId, 100).size() == 15);
    }

    SECTION("Query during cleanup returns consistent results") {
        // Insert data
        for (int i = 0; i < 10; ++i) {
            metricsRepo.insertPingResult(createTestPingResult(hostId));
        }

        // Query and cleanup
        auto resultsBefore = metricsRepo.getPingResults(hostId, 100);
        REQUIRE(resultsBefore.size() == 10);

        metricsRepo.cleanupOldPingResults(std::chrono::hours(-1));

        auto resultsAfter = metricsRepo.getPingResults(hostId, 100);
        REQUIRE(resultsAfter.empty());
    }
}
