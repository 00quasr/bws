#include <catch2/catch_approx.hpp>
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

using namespace netpulse::infra;
using namespace netpulse::core;

namespace {

class TestDatabase {
public:
    TestDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_metricsrepo_test.db") {
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
// Ping Results Tests
// =============================================================================

TEST_CASE("MetricsRepository ping result insert operations", "[MetricsRepository][PingResults]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    // Create a host to satisfy foreign key constraint
    int64_t hostId = hostRepo.insert(createTestHost("Test Host", "192.168.1.1"));

    SECTION("Insert ping result returns positive ID") {
        PingResult result = createTestPingResult(hostId);
        int64_t id = repo.insertPingResult(result);
        REQUIRE(id > 0);
    }

    SECTION("Insert successful ping result") {
        PingResult result = createTestPingResult(hostId, true, std::chrono::microseconds(3500));
        result.ttl = 128;
        int64_t id = repo.insertPingResult(result);

        auto results = repo.getPingResults(hostId, 1);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].id == id);
        REQUIRE(results[0].hostId == hostId);
        REQUIRE(results[0].success == true);
        REQUIRE(results[0].latency == std::chrono::microseconds(3500));
        REQUIRE(results[0].ttl.has_value());
        REQUIRE(*results[0].ttl == 128);
    }

    SECTION("Insert failed ping result") {
        PingResult result = createTestPingResult(hostId, false, std::chrono::microseconds(0));
        result.ttl = std::nullopt;
        int64_t id = repo.insertPingResult(result);

        auto results = repo.getPingResults(hostId, 1);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].id == id);
        REQUIRE(results[0].success == false);
        REQUIRE_FALSE(results[0].ttl.has_value());
    }

    SECTION("Insert multiple ping results returns unique IDs") {
        PingResult r1 = createTestPingResult(hostId);
        PingResult r2 = createTestPingResult(hostId);
        PingResult r3 = createTestPingResult(hostId);

        int64_t id1 = repo.insertPingResult(r1);
        int64_t id2 = repo.insertPingResult(r2);
        int64_t id3 = repo.insertPingResult(r3);

        REQUIRE(id1 > 0);
        REQUIRE(id2 > id1);
        REQUIRE(id3 > id2);
    }
}

TEST_CASE("MetricsRepository ping result read operations", "[MetricsRepository][PingResults]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId1 = hostRepo.insert(createTestHost("Host 1", "192.168.1.1"));
    int64_t hostId2 = hostRepo.insert(createTestHost("Host 2", "192.168.1.2"));

    SECTION("getPingResults returns results for specified host") {
        repo.insertPingResult(createTestPingResult(hostId1));
        repo.insertPingResult(createTestPingResult(hostId1));
        repo.insertPingResult(createTestPingResult(hostId2));

        auto host1Results = repo.getPingResults(hostId1, 100);
        auto host2Results = repo.getPingResults(hostId2, 100);

        REQUIRE(host1Results.size() == 2);
        REQUIRE(host2Results.size() == 1);
        for (const auto& r : host1Results) {
            REQUIRE(r.hostId == hostId1);
        }
        REQUIRE(host2Results[0].hostId == hostId2);
    }

    SECTION("getPingResults respects limit parameter") {
        for (int i = 0; i < 10; ++i) {
            repo.insertPingResult(createTestPingResult(hostId1));
        }

        auto limited = repo.getPingResults(hostId1, 5);
        REQUIRE(limited.size() == 5);
    }

    SECTION("getPingResults returns results ordered by timestamp DESC") {
        for (int i = 0; i < 5; ++i) {
            repo.insertPingResult(createTestPingResult(hostId1));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto results = repo.getPingResults(hostId1, 100);
        REQUIRE(results.size() == 5);
        for (size_t i = 1; i < results.size(); ++i) {
            REQUIRE(results[i - 1].timestamp >= results[i].timestamp);
        }
    }

    SECTION("getPingResults returns empty vector for non-existent host") {
        auto results = repo.getPingResults(99999, 100);
        REQUIRE(results.empty());
    }

    SECTION("getPingResultsSince filters by timestamp") {
        auto oldTime = std::chrono::system_clock::now() - std::chrono::seconds(10);

        repo.insertPingResult(createTestPingResult(hostId1));
        repo.insertPingResult(createTestPingResult(hostId1));

        // Get all results inserted so far
        auto allResults = repo.getPingResultsSince(hostId1, oldTime);
        REQUIRE(allResults.size() == 2);

        // Use a future timestamp to get zero results
        auto futureTime = std::chrono::system_clock::now() + std::chrono::seconds(10);
        auto noResults = repo.getPingResultsSince(hostId1, futureTime);
        REQUIRE(noResults.empty());
    }

    SECTION("getPingResultsSince returns results ordered by timestamp ASC") {
        for (int i = 0; i < 5; ++i) {
            repo.insertPingResult(createTestPingResult(hostId1));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto since = std::chrono::system_clock::now() - std::chrono::hours(1);
        auto results = repo.getPingResultsSince(hostId1, since);
        REQUIRE(results.size() == 5);
        for (size_t i = 1; i < results.size(); ++i) {
            REQUIRE(results[i - 1].timestamp <= results[i].timestamp);
        }
    }
}

TEST_CASE("MetricsRepository ping statistics", "[MetricsRepository][PingResults][Statistics]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Stats Host", "192.168.1.1"));

    SECTION("getStatistics with no data returns zero values") {
        auto stats = repo.getStatistics(hostId);
        REQUIRE(stats.hostId == hostId);
        REQUIRE(stats.totalPings == 0);
        REQUIRE(stats.successfulPings == 0);
        REQUIRE(stats.packetLossPercent == 0.0);
    }

    SECTION("getStatistics calculates correct counts") {
        for (int i = 0; i < 8; ++i) {
            repo.insertPingResult(createTestPingResult(hostId, true));
        }
        for (int i = 0; i < 2; ++i) {
            repo.insertPingResult(createTestPingResult(hostId, false));
        }

        auto stats = repo.getStatistics(hostId);
        REQUIRE(stats.totalPings == 10);
        REQUIRE(stats.successfulPings == 8);
        REQUIRE(stats.packetLossPercent == Catch::Approx(20.0));
    }

    SECTION("getStatistics calculates min/max/avg latency") {
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(1000)));
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(2000)));
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(3000)));

        auto stats = repo.getStatistics(hostId);
        REQUIRE(stats.minLatency == std::chrono::microseconds(1000));
        REQUIRE(stats.maxLatency == std::chrono::microseconds(3000));
        REQUIRE(stats.avgLatency == std::chrono::microseconds(2000));
    }

    SECTION("getStatistics ignores failed pings for latency calculations") {
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(1000)));
        repo.insertPingResult(createTestPingResult(hostId, false, std::chrono::microseconds(999999)));
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(3000)));

        auto stats = repo.getStatistics(hostId);
        REQUIRE(stats.minLatency == std::chrono::microseconds(1000));
        REQUIRE(stats.maxLatency == std::chrono::microseconds(3000));
    }

    SECTION("getStatistics respects sampleCount parameter") {
        for (int i = 0; i < 20; ++i) {
            repo.insertPingResult(createTestPingResult(hostId, true));
        }

        auto stats10 = repo.getStatistics(hostId, 10);
        auto stats20 = repo.getStatistics(hostId, 20);

        REQUIRE(stats10.totalPings == 10);
        REQUIRE(stats20.totalPings == 20);
    }

    SECTION("getStatistics calculates jitter for successful pings") {
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(1000)));
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(2000)));
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(3000)));

        auto stats = repo.getStatistics(hostId);
        REQUIRE(stats.jitter.count() > 0);
    }
}

TEST_CASE("MetricsRepository ping cleanup", "[MetricsRepository][PingResults][Cleanup]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Cleanup Host", "192.168.1.1"));

    SECTION("cleanupOldPingResults removes old entries") {
        for (int i = 0; i < 5; ++i) {
            repo.insertPingResult(createTestPingResult(hostId));
        }

        auto beforeCleanup = repo.getPingResults(hostId, 100);
        REQUIRE(beforeCleanup.size() == 5);

        // Use a future time to clean up all entries
        repo.cleanupOldPingResults(std::chrono::hours(-1));

        auto afterCleanup = repo.getPingResults(hostId, 100);
        REQUIRE(afterCleanup.empty());
    }
}

// =============================================================================
// Alert Tests
// =============================================================================

TEST_CASE("MetricsRepository alert insert operations", "[MetricsRepository][Alerts]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    SECTION("Insert alert returns positive ID") {
        Alert alert = createTestAlert(1);
        int64_t id = repo.insertAlert(alert);
        REQUIRE(id > 0);
    }

    SECTION("Insert alert with all fields") {
        Alert alert;
        alert.hostId = 42;
        alert.type = AlertType::HighLatency;
        alert.severity = AlertSeverity::Warning;
        alert.title = "High Latency Detected";
        alert.message = "Latency exceeded threshold";
        alert.timestamp = std::chrono::system_clock::now();
        alert.acknowledged = false;

        int64_t id = repo.insertAlert(alert);
        auto alerts = repo.getAlerts(1);

        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].id == id);
        REQUIRE(alerts[0].hostId == 42);
        REQUIRE(alerts[0].type == AlertType::HighLatency);
        REQUIRE(alerts[0].severity == AlertSeverity::Warning);
        REQUIRE(alerts[0].title == "High Latency Detected");
        REQUIRE(alerts[0].message == "Latency exceeded threshold");
        REQUIRE(alerts[0].acknowledged == false);
    }

    SECTION("Insert alerts with different types") {
        repo.insertAlert(createTestAlert(1, AlertType::HostDown, AlertSeverity::Critical));
        repo.insertAlert(createTestAlert(1, AlertType::HighLatency, AlertSeverity::Warning));
        repo.insertAlert(createTestAlert(1, AlertType::PacketLoss, AlertSeverity::Warning));
        repo.insertAlert(createTestAlert(1, AlertType::HostRecovered, AlertSeverity::Info));
        repo.insertAlert(createTestAlert(1, AlertType::ScanComplete, AlertSeverity::Info));

        auto alerts = repo.getAlerts(100);
        REQUIRE(alerts.size() == 5);
    }
}

TEST_CASE("MetricsRepository alert read operations", "[MetricsRepository][Alerts]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    SECTION("getAlerts returns all alerts ordered by timestamp DESC") {
        for (int i = 0; i < 5; ++i) {
            repo.insertAlert(createTestAlert(1));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto alerts = repo.getAlerts(100);
        REQUIRE(alerts.size() == 5);
        for (size_t i = 1; i < alerts.size(); ++i) {
            REQUIRE(alerts[i - 1].timestamp >= alerts[i].timestamp);
        }
    }

    SECTION("getAlerts respects limit parameter") {
        for (int i = 0; i < 10; ++i) {
            repo.insertAlert(createTestAlert(1));
        }

        auto limited = repo.getAlerts(5);
        REQUIRE(limited.size() == 5);
    }

    SECTION("getUnacknowledgedAlerts returns only unacknowledged") {
        Alert ack = createTestAlert(1);
        ack.acknowledged = true;
        Alert unack = createTestAlert(1);
        unack.acknowledged = false;

        repo.insertAlert(ack);
        repo.insertAlert(unack);
        repo.insertAlert(unack);

        auto unacknowledged = repo.getUnacknowledgedAlerts();
        REQUIRE(unacknowledged.size() == 2);
        for (const auto& alert : unacknowledged) {
            REQUIRE(alert.acknowledged == false);
        }
    }
}

TEST_CASE("MetricsRepository alert filtering", "[MetricsRepository][Alerts][Filter]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    // Set up test data
    repo.insertAlert(createTestAlert(1, AlertType::HostDown, AlertSeverity::Critical));
    repo.insertAlert(createTestAlert(1, AlertType::HighLatency, AlertSeverity::Warning));
    repo.insertAlert(createTestAlert(2, AlertType::PacketLoss, AlertSeverity::Warning));
    repo.insertAlert(createTestAlert(2, AlertType::HostRecovered, AlertSeverity::Info));

    SECTION("Filter by severity") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Warning;

        auto results = repo.getAlertsFiltered(filter, 100);
        REQUIRE(results.size() == 2);
        for (const auto& alert : results) {
            REQUIRE(alert.severity == AlertSeverity::Warning);
        }
    }

    SECTION("Filter by type") {
        AlertFilter filter;
        filter.type = AlertType::HostDown;

        auto results = repo.getAlertsFiltered(filter, 100);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].type == AlertType::HostDown);
    }

    SECTION("Filter by acknowledged status") {
        auto id = repo.insertAlert(createTestAlert(1));
        repo.acknowledgeAlert(id);

        AlertFilter filterAck;
        filterAck.acknowledged = true;
        auto ackResults = repo.getAlertsFiltered(filterAck, 100);
        REQUIRE(ackResults.size() == 1);

        AlertFilter filterUnack;
        filterUnack.acknowledged = false;
        auto unackResults = repo.getAlertsFiltered(filterUnack, 100);
        REQUIRE(unackResults.size() == 4);
    }

    SECTION("Filter by search text in title") {
        Alert specialAlert = createTestAlert(1);
        specialAlert.title = "UniqueSearchableTitle";
        repo.insertAlert(specialAlert);

        AlertFilter filter;
        filter.searchText = "UniqueSearchable";

        auto results = repo.getAlertsFiltered(filter, 100);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].title == "UniqueSearchableTitle");
    }

    SECTION("Filter by search text in message") {
        Alert specialAlert = createTestAlert(1);
        specialAlert.message = "This contains SpecialKeyword in message";
        repo.insertAlert(specialAlert);

        AlertFilter filter;
        filter.searchText = "SpecialKeyword";

        auto results = repo.getAlertsFiltered(filter, 100);
        REQUIRE(results.size() == 1);
    }

    SECTION("Combined filters") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Warning;
        filter.type = AlertType::HighLatency;

        auto results = repo.getAlertsFiltered(filter, 100);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].severity == AlertSeverity::Warning);
        REQUIRE(results[0].type == AlertType::HighLatency);
    }

    SECTION("Empty filter returns all alerts") {
        AlertFilter filter;
        auto results = repo.getAlertsFiltered(filter, 100);
        REQUIRE(results.size() == 4);
    }
}

TEST_CASE("MetricsRepository alert acknowledgment", "[MetricsRepository][Alerts]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    SECTION("acknowledgeAlert marks single alert as acknowledged") {
        Alert alert = createTestAlert(1);
        int64_t id = repo.insertAlert(alert);

        auto before = repo.getUnacknowledgedAlerts();
        REQUIRE(before.size() == 1);

        repo.acknowledgeAlert(id);

        auto after = repo.getUnacknowledgedAlerts();
        REQUIRE(after.empty());
    }

    SECTION("acknowledgeAll marks all alerts as acknowledged") {
        for (int i = 0; i < 5; ++i) {
            repo.insertAlert(createTestAlert(1));
        }

        auto before = repo.getUnacknowledgedAlerts();
        REQUIRE(before.size() == 5);

        repo.acknowledgeAll();

        auto after = repo.getUnacknowledgedAlerts();
        REQUIRE(after.empty());
    }

    SECTION("acknowledgeAlert on non-existent ID does not throw") {
        REQUIRE_NOTHROW(repo.acknowledgeAlert(99999));
    }
}

TEST_CASE("MetricsRepository alert cleanup", "[MetricsRepository][Alerts][Cleanup]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    SECTION("cleanupOldAlerts removes old entries") {
        for (int i = 0; i < 5; ++i) {
            repo.insertAlert(createTestAlert(1));
        }

        auto before = repo.getAlerts(100);
        REQUIRE(before.size() == 5);

        repo.cleanupOldAlerts(std::chrono::hours(-1));

        auto after = repo.getAlerts(100);
        REQUIRE(after.empty());
    }
}

// =============================================================================
// Port Scan Results Tests
// =============================================================================

TEST_CASE("MetricsRepository port scan insert operations", "[MetricsRepository][PortScans]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    SECTION("Insert port scan result returns positive ID") {
        PortScanResult result = createTestPortScanResult("192.168.1.1", 80);
        int64_t id = repo.insertPortScanResult(result);
        REQUIRE(id > 0);
    }

    SECTION("Insert port scan result with all fields") {
        PortScanResult result;
        result.targetAddress = "10.0.0.1";
        result.port = 443;
        result.state = PortState::Open;
        result.serviceName = "https";
        result.scanTimestamp = std::chrono::system_clock::now();

        int64_t id = repo.insertPortScanResult(result);
        auto results = repo.getPortScanResults("10.0.0.1", 10);

        REQUIRE(results.size() == 1);
        REQUIRE(results[0].id == id);
        REQUIRE(results[0].targetAddress == "10.0.0.1");
        REQUIRE(results[0].port == 443);
        REQUIRE(results[0].state == PortState::Open);
        REQUIRE(results[0].serviceName == "https");
    }

    SECTION("Insert port scan results with different states") {
        repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", 80, PortState::Open));
        repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", 81, PortState::Closed));
        repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", 82, PortState::Filtered));
        repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", 83, PortState::Unknown));

        auto results = repo.getPortScanResults("192.168.1.1", 100);
        REQUIRE(results.size() == 4);
    }

    SECTION("insertPortScanResults batch insert") {
        std::vector<PortScanResult> results;
        results.push_back(createTestPortScanResult("192.168.1.1", 22));
        results.push_back(createTestPortScanResult("192.168.1.1", 80));
        results.push_back(createTestPortScanResult("192.168.1.1", 443));

        REQUIRE_NOTHROW(repo.insertPortScanResults(results));

        auto retrieved = repo.getPortScanResults("192.168.1.1", 100);
        REQUIRE(retrieved.size() == 3);
    }
}

TEST_CASE("MetricsRepository port scan read operations", "[MetricsRepository][PortScans]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    SECTION("getPortScanResults returns results for specified address") {
        repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", 80));
        repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", 443));
        repo.insertPortScanResult(createTestPortScanResult("192.168.1.2", 80));

        auto addr1Results = repo.getPortScanResults("192.168.1.1", 100);
        auto addr2Results = repo.getPortScanResults("192.168.1.2", 100);

        REQUIRE(addr1Results.size() == 2);
        REQUIRE(addr2Results.size() == 1);
    }

    SECTION("getPortScanResults respects limit parameter") {
        for (int port = 1; port <= 20; ++port) {
            repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", static_cast<uint16_t>(port)));
        }

        auto limited = repo.getPortScanResults("192.168.1.1", 10);
        REQUIRE(limited.size() == 10);
    }

    SECTION("getPortScanResults returns empty for non-existent address") {
        auto results = repo.getPortScanResults("255.255.255.255", 100);
        REQUIRE(results.empty());
    }
}

TEST_CASE("MetricsRepository port scan cleanup", "[MetricsRepository][PortScans][Cleanup]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    SECTION("cleanupOldPortScans removes old entries") {
        for (int port = 1; port <= 5; ++port) {
            repo.insertPortScanResult(createTestPortScanResult("192.168.1.1", static_cast<uint16_t>(port)));
        }

        auto before = repo.getPortScanResults("192.168.1.1", 100);
        REQUIRE(before.size() == 5);

        repo.cleanupOldPortScans(std::chrono::hours(-1));

        auto after = repo.getPortScanResults("192.168.1.1", 100);
        REQUIRE(after.empty());
    }
}

// =============================================================================
// Export Tests
// =============================================================================

TEST_CASE("MetricsRepository export operations", "[MetricsRepository][Export]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Export Host", "192.168.1.1"));

    SECTION("exportToJson returns valid JSON with ping results") {
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(5000)));
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(6000)));

        std::string json = repo.exportToJson(hostId);

        REQUIRE_FALSE(json.empty());
        // JSON is pretty-printed with spaces after colons
        REQUIRE(json.find("\"host_id\": " + std::to_string(hostId)) != std::string::npos);
        REQUIRE(json.find("\"ping_results\"") != std::string::npos);
        REQUIRE(json.find("\"statistics\"") != std::string::npos);
    }

    SECTION("exportToJson includes statistics") {
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(5000)));

        std::string json = repo.exportToJson(hostId);

        REQUIRE(json.find("\"total_pings\"") != std::string::npos);
        REQUIRE(json.find("\"successful_pings\"") != std::string::npos);
        REQUIRE(json.find("\"packet_loss_percent\"") != std::string::npos);
        REQUIRE(json.find("\"min_latency_ms\"") != std::string::npos);
        REQUIRE(json.find("\"max_latency_ms\"") != std::string::npos);
        REQUIRE(json.find("\"avg_latency_ms\"") != std::string::npos);
        REQUIRE(json.find("\"jitter_ms\"") != std::string::npos);
    }

    SECTION("exportToJson with no data returns valid JSON structure") {
        int64_t emptyHostId = hostRepo.insert(createTestHost("Empty Host", "192.168.1.2"));
        std::string json = repo.exportToJson(emptyHostId);

        REQUIRE_FALSE(json.empty());
        // JSON is pretty-printed with spaces after colons
        REQUIRE(json.find("\"host_id\": " + std::to_string(emptyHostId)) != std::string::npos);
        REQUIRE(json.find("\"ping_results\": []") != std::string::npos);
    }

    SECTION("exportToCsv returns valid CSV with header") {
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(5000)));

        std::string csv = repo.exportToCsv(hostId);

        REQUIRE_FALSE(csv.empty());
        REQUIRE(csv.find("timestamp,latency_ms,success,ttl") != std::string::npos);
    }

    SECTION("exportToCsv includes data rows") {
        repo.insertPingResult(createTestPingResult(hostId, true, std::chrono::microseconds(5000)));
        repo.insertPingResult(createTestPingResult(hostId, false, std::chrono::microseconds(0)));

        std::string csv = repo.exportToCsv(hostId);

        // Should have header + 2 data rows
        int newlines = static_cast<int>(std::count(csv.begin(), csv.end(), '\n'));
        REQUIRE(newlines >= 3);
        REQUIRE(csv.find("true") != std::string::npos);
        REQUIRE(csv.find("false") != std::string::npos);
    }

    SECTION("exportToCsv with no data returns only header") {
        int64_t emptyHostId = hostRepo.insert(createTestHost("Empty CSV Host", "192.168.1.3"));
        std::string csv = repo.exportToCsv(emptyHostId);

        REQUIRE_FALSE(csv.empty());
        REQUIRE(csv.find("timestamp,latency_ms,success,ttl") != std::string::npos);
        int newlines = static_cast<int>(std::count(csv.begin(), csv.end(), '\n'));
        REQUIRE(newlines == 1);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("MetricsRepository edge cases", "[MetricsRepository][EdgeCases]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createTestHost("Edge Case Host", "192.168.1.1"));

    SECTION("Handle zero latency") {
        PingResult result = createTestPingResult(hostId, true, std::chrono::microseconds(0));
        repo.insertPingResult(result);

        auto results = repo.getPingResults(hostId, 1);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].latency == std::chrono::microseconds(0));
    }

    SECTION("Handle very high latency") {
        PingResult result = createTestPingResult(hostId, true, std::chrono::microseconds(999999999));
        repo.insertPingResult(result);

        auto results = repo.getPingResults(hostId, 1);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].latency == std::chrono::microseconds(999999999));
    }

    SECTION("Handle special characters in alert title and message") {
        Alert alert = createTestAlert(1);
        alert.title = "Test's \"Alert\" <with> Special & Characters";
        alert.message = "Message with 日本語 and émojis 🔔";

        int64_t id = repo.insertAlert(alert);
        auto alerts = repo.getAlerts(1);

        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].title == "Test's \"Alert\" <with> Special & Characters");
        REQUIRE(alerts[0].message == "Message with 日本語 and émojis 🔔");
    }

    SECTION("Handle IPv6 address in port scan") {
        PortScanResult result = createTestPortScanResult("2001:db8::1", 80);
        repo.insertPortScanResult(result);

        auto results = repo.getPortScanResults("2001:db8::1", 10);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].targetAddress == "2001:db8::1");
    }

    SECTION("Handle hostname in port scan") {
        PortScanResult result = createTestPortScanResult("example.com", 443);
        repo.insertPortScanResult(result);

        auto results = repo.getPortScanResults("example.com", 10);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].targetAddress == "example.com");
    }

    SECTION("Handle port 0 in port scan") {
        PortScanResult result = createTestPortScanResult("192.168.1.1", 0);
        repo.insertPortScanResult(result);

        auto results = repo.getPortScanResults("192.168.1.1", 10);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].port == 0);
    }

    SECTION("Handle max port 65535 in port scan") {
        PortScanResult result = createTestPortScanResult("192.168.1.1", 65535);
        repo.insertPortScanResult(result);

        auto results = repo.getPortScanResults("192.168.1.1", 10);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].port == 65535);
    }

    SECTION("Multiple hosts with interleaved data") {
        int64_t hostId2 = hostRepo.insert(createTestHost("Host 2", "192.168.1.2"));
        int64_t hostId3 = hostRepo.insert(createTestHost("Host 3", "192.168.1.3"));

        for (int i = 0; i < 5; ++i) {
            repo.insertPingResult(createTestPingResult(hostId));
            repo.insertPingResult(createTestPingResult(hostId2));
            repo.insertPingResult(createTestPingResult(hostId3));
        }

        REQUIRE(repo.getPingResults(hostId, 100).size() == 5);
        REQUIRE(repo.getPingResults(hostId2, 100).size() == 5);
        REQUIRE(repo.getPingResults(hostId3, 100).size() == 5);
    }
}
