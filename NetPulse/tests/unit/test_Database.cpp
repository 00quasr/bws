#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"

#include <filesystem>

using namespace netpulse::infra;
using namespace netpulse::core;

class TestDatabase {
public:
    TestDatabase() : dbPath_(std::filesystem::temp_directory_path() / "netpulse_test.db") {
        // Remove existing test database
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

TEST_CASE("Database operations", "[Database]") {
    TestDatabase testDb;
    auto db = testDb.get();

    SECTION("Execute simple SQL") {
        REQUIRE_NOTHROW(db->execute("SELECT 1"));
    }

    SECTION("Prepare and execute statement") {
        auto stmt = db->prepare("SELECT 1 + 1");
        REQUIRE(stmt.step());
        REQUIRE(stmt.columnInt(0) == 2);
    }

    SECTION("Transaction commit") {
        db->beginTransaction();
        db->execute("CREATE TABLE test_tx (id INTEGER PRIMARY KEY, value TEXT)");
        db->execute("INSERT INTO test_tx (value) VALUES ('test')");
        db->commit();

        auto stmt = db->prepare("SELECT value FROM test_tx");
        REQUIRE(stmt.step());
        REQUIRE(stmt.columnText(0) == "test");
    }

    SECTION("Transaction rollback") {
        db->execute("CREATE TABLE test_rb (id INTEGER PRIMARY KEY, value TEXT)");

        db->beginTransaction();
        db->execute("INSERT INTO test_rb (value) VALUES ('test')");
        db->rollback();

        auto stmt = db->prepare("SELECT COUNT(*) FROM test_rb");
        REQUIRE(stmt.step());
        REQUIRE(stmt.columnInt(0) == 0);
    }
}

TEST_CASE("HostRepository operations", "[Database][HostRepository]") {
    TestDatabase testDb;
    HostRepository repo(testDb.get());

    SECTION("Insert and retrieve host") {
        Host host;
        host.name = "Test Host";
        host.address = "192.168.1.1";
        host.pingIntervalSeconds = 30;
        host.warningThresholdMs = 100;
        host.criticalThresholdMs = 500;
        host.status = HostStatus::Unknown;
        host.enabled = true;
        host.createdAt = std::chrono::system_clock::now();

        int64_t id = repo.insert(host);
        REQUIRE(id > 0);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Test Host");
        REQUIRE(retrieved->address == "192.168.1.1");
    }

    SECTION("Find by address") {
        Host host;
        host.name = "Test Host";
        host.address = "10.0.0.1";
        host.createdAt = std::chrono::system_clock::now();

        repo.insert(host);

        auto found = repo.findByAddress("10.0.0.1");
        REQUIRE(found.has_value());
        REQUIRE(found->name == "Test Host");

        auto notFound = repo.findByAddress("10.0.0.2");
        REQUIRE_FALSE(notFound.has_value());
    }

    SECTION("Update host") {
        Host host;
        host.name = "Original Name";
        host.address = "172.16.0.1";
        host.createdAt = std::chrono::system_clock::now();

        int64_t id = repo.insert(host);

        auto retrieved = repo.findById(id);
        retrieved->name = "Updated Name";
        repo.update(*retrieved);

        auto updated = repo.findById(id);
        REQUIRE(updated->name == "Updated Name");
    }

    SECTION("Remove host") {
        Host host;
        host.name = "To Remove";
        host.address = "192.168.100.1";
        host.createdAt = std::chrono::system_clock::now();

        int64_t id = repo.insert(host);
        REQUIRE(repo.findById(id).has_value());

        repo.remove(id);
        REQUIRE_FALSE(repo.findById(id).has_value());
    }

    SECTION("Find all and count") {
        Host host1;
        host1.name = "Host 1";
        host1.address = "1.1.1.1";
        host1.createdAt = std::chrono::system_clock::now();

        Host host2;
        host2.name = "Host 2";
        host2.address = "2.2.2.2";
        host2.createdAt = std::chrono::system_clock::now();

        repo.insert(host1);
        repo.insert(host2);

        auto all = repo.findAll();
        REQUIRE(all.size() == 2);
        REQUIRE(repo.count() == 2);
    }

    SECTION("Find enabled hosts") {
        Host enabled;
        enabled.name = "Enabled";
        enabled.address = "3.3.3.3";
        enabled.enabled = true;
        enabled.createdAt = std::chrono::system_clock::now();

        Host disabled;
        disabled.name = "Disabled";
        disabled.address = "4.4.4.4";
        disabled.enabled = false;
        disabled.createdAt = std::chrono::system_clock::now();

        repo.insert(enabled);
        repo.insert(disabled);

        auto enabledHosts = repo.findEnabled();
        REQUIRE(enabledHosts.size() == 1);
        REQUIRE(enabledHosts[0].name == "Enabled");
    }
}

TEST_CASE("MetricsRepository operations", "[Database][MetricsRepository]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository metricsRepo(testDb.get());

    // Create a test host
    Host host;
    host.name = "Metrics Test";
    host.address = "5.5.5.5";
    host.createdAt = std::chrono::system_clock::now();
    int64_t hostId = hostRepo.insert(host);

    SECTION("Insert and retrieve ping results") {
        PingResult result;
        result.hostId = hostId;
        result.timestamp = std::chrono::system_clock::now();
        result.latency = std::chrono::microseconds(15000);
        result.success = true;
        result.ttl = 64;

        int64_t id = metricsRepo.insertPingResult(result);
        REQUIRE(id > 0);

        auto results = metricsRepo.getPingResults(hostId, 10);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].latencyMs() == 15.0);
        REQUIRE(results[0].ttl == 64);
    }

    SECTION("Calculate statistics") {
        // Insert multiple results
        for (int i = 0; i < 10; ++i) {
            PingResult result;
            result.hostId = hostId;
            result.timestamp = std::chrono::system_clock::now();
            result.latency = std::chrono::microseconds(10000 + i * 1000);
            result.success = (i < 8);  // 2 failures
            metricsRepo.insertPingResult(result);
        }

        auto stats = metricsRepo.getStatistics(hostId, 10);
        REQUIRE(stats.totalPings == 10);
        REQUIRE(stats.successfulPings == 8);
        REQUIRE(stats.packetLossPercent == Catch::Approx(20.0).epsilon(0.01));
    }

    SECTION("Insert and retrieve alerts") {
        Alert alert;
        alert.hostId = hostId;
        alert.type = AlertType::HighLatency;
        alert.severity = AlertSeverity::Warning;
        alert.title = "Test Alert";
        alert.message = "Test message";
        alert.timestamp = std::chrono::system_clock::now();
        alert.acknowledged = false;

        int64_t id = metricsRepo.insertAlert(alert);
        REQUIRE(id > 0);

        auto alerts = metricsRepo.getAlerts(10);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].title == "Test Alert");

        auto unack = metricsRepo.getUnacknowledgedAlerts();
        REQUIRE(unack.size() == 1);

        metricsRepo.acknowledgeAlert(id);
        unack = metricsRepo.getUnacknowledgedAlerts();
        REQUIRE(unack.empty());
    }

    SECTION("Export to JSON") {
        PingResult result;
        result.hostId = hostId;
        result.timestamp = std::chrono::system_clock::now();
        result.latency = std::chrono::microseconds(25000);
        result.success = true;
        metricsRepo.insertPingResult(result);

        std::string json = metricsRepo.exportToJson(hostId);
        REQUIRE_FALSE(json.empty());
        REQUIRE(json.find("host_id") != std::string::npos);
        REQUIRE(json.find("ping_results") != std::string::npos);
    }

    SECTION("Export to CSV") {
        PingResult result;
        result.hostId = hostId;
        result.timestamp = std::chrono::system_clock::now();
        result.latency = std::chrono::microseconds(25000);
        result.success = true;
        metricsRepo.insertPingResult(result);

        std::string csv = metricsRepo.exportToCsv(hostId);
        REQUIRE_FALSE(csv.empty());
        REQUIRE(csv.find("timestamp,latency_ms,success,ttl") != std::string::npos);
    }
}
