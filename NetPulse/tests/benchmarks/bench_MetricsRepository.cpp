#include <catch2/benchmark/catch_benchmark.hpp>
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
#include <memory>
#include <string>
#include <vector>

using namespace netpulse::infra;
using namespace netpulse::core;

namespace {

class BenchmarkDatabase {
public:
    BenchmarkDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_bench_metrics.db") {
        std::filesystem::remove(dbPath_);
        db_ = std::make_shared<Database>(dbPath_.string());
        db_->runMigrations();
    }

    ~BenchmarkDatabase() {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::shared_ptr<Database> get() { return db_; }

private:
    std::filesystem::path dbPath_;
    std::shared_ptr<Database> db_;
};

Host createBenchHost(const std::string& name, const std::string& address) {
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

PingResult createBenchPingResult(int64_t hostId, bool success = true,
                                  std::chrono::microseconds latency = std::chrono::microseconds(5000)) {
    PingResult result;
    result.hostId = hostId;
    result.timestamp = std::chrono::system_clock::now();
    result.latency = latency;
    result.success = success;
    result.ttl = 64;
    return result;
}

Alert createBenchAlert(int64_t hostId, AlertType type = AlertType::HostDown,
                       AlertSeverity severity = AlertSeverity::Critical) {
    Alert alert;
    alert.hostId = hostId;
    alert.type = type;
    alert.severity = severity;
    alert.title = "Benchmark Alert";
    alert.message = "This is a benchmark alert message";
    alert.timestamp = std::chrono::system_clock::now();
    alert.acknowledged = false;
    return alert;
}

PortScanResult createBenchPortScanResult(const std::string& address, uint16_t port,
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
// Ping Result Insert Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository ping result insert benchmarks", "[benchmark][MetricsRepository][insert]") {
    BenchmarkDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createBenchHost("Benchmark Host", "192.168.1.1"));

    BENCHMARK("Insert single ping result") {
        PingResult result = createBenchPingResult(hostId);
        return repo.insertPingResult(result);
    };

    BENCHMARK("Insert ping result with all fields") {
        PingResult result;
        result.hostId = hostId;
        result.timestamp = std::chrono::system_clock::now();
        result.latency = std::chrono::microseconds{12345};
        result.success = true;
        result.ttl = 128;
        result.errorMessage = "";
        return repo.insertPingResult(result);
    };

    BENCHMARK("Insert failed ping result") {
        PingResult result;
        result.hostId = hostId;
        result.timestamp = std::chrono::system_clock::now();
        result.latency = std::chrono::microseconds{0};
        result.success = false;
        result.ttl = std::nullopt;
        result.errorMessage = "Connection timeout";
        return repo.insertPingResult(result);
    };
}

// =============================================================================
// Ping Result Read Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository ping result read benchmarks", "[benchmark][MetricsRepository][read]") {
    BenchmarkDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createBenchHost("Benchmark Host", "192.168.1.1"));

    // Pre-populate with test data
    for (int i = 0; i < 1000; ++i) {
        repo.insertPingResult(createBenchPingResult(
            hostId, (i % 10 != 0), std::chrono::microseconds{1000 + i * 10}));
    }

    BENCHMARK("getPingResults limit 10") {
        return repo.getPingResults(hostId, 10);
    };

    BENCHMARK("getPingResults limit 100") {
        return repo.getPingResults(hostId, 100);
    };

    BENCHMARK("getPingResults limit 500") {
        return repo.getPingResults(hostId, 500);
    };

    auto since = std::chrono::system_clock::now() - std::chrono::hours(1);
    BENCHMARK("getPingResultsSince (all results)") {
        return repo.getPingResultsSince(hostId, since);
    };

    auto recentSince = std::chrono::system_clock::now() - std::chrono::seconds(1);
    BENCHMARK("getPingResultsSince (recent only)") {
        return repo.getPingResultsSince(hostId, recentSince);
    };
}

// =============================================================================
// Statistics Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository statistics benchmarks", "[benchmark][MetricsRepository][statistics]") {
    BenchmarkDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createBenchHost("Stats Host", "192.168.1.1"));

    // Pre-populate with varied latency data
    for (int i = 0; i < 500; ++i) {
        bool success = (i % 10 != 0);
        auto latency = std::chrono::microseconds{1000 + (i % 100) * 500};
        repo.insertPingResult(createBenchPingResult(hostId, success, latency));
    }

    BENCHMARK("getStatistics default sample") {
        return repo.getStatistics(hostId);
    };

    BENCHMARK("getStatistics sample 100") {
        return repo.getStatistics(hostId, 100);
    };

    BENCHMARK("getStatistics sample 500") {
        return repo.getStatistics(hostId, 500);
    };
}

// =============================================================================
// Alert Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository alert benchmarks", "[benchmark][MetricsRepository][alerts]") {
    BenchmarkDatabase testDb;
    MetricsRepository repo(testDb.get());

    BENCHMARK("Insert single alert") {
        Alert alert = createBenchAlert(1);
        return repo.insertAlert(alert);
    };

    // Pre-populate for read benchmarks
    for (int i = 0; i < 200; ++i) {
        Alert alert;
        alert.hostId = i % 10;
        alert.type = static_cast<AlertType>(i % 5);
        alert.severity = static_cast<AlertSeverity>(i % 3);
        alert.title = "Alert " + std::to_string(i);
        alert.message = "Message " + std::to_string(i);
        alert.timestamp = std::chrono::system_clock::now();
        alert.acknowledged = (i % 4 == 0);
        repo.insertAlert(alert);
    }

    BENCHMARK("getAlerts limit 10") {
        return repo.getAlerts(10);
    };

    BENCHMARK("getAlerts limit 50") {
        return repo.getAlerts(50);
    };

    BENCHMARK("getAlerts limit 100") {
        return repo.getAlerts(100);
    };

    BENCHMARK("getUnacknowledgedAlerts") {
        return repo.getUnacknowledgedAlerts();
    };
}

// =============================================================================
// Alert Filter Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository alert filter benchmarks", "[benchmark][MetricsRepository][filter]") {
    BenchmarkDatabase testDb;
    MetricsRepository repo(testDb.get());

    // Pre-populate with diverse alerts
    for (int i = 0; i < 300; ++i) {
        Alert alert;
        alert.hostId = i % 20;
        alert.type = static_cast<AlertType>(i % 5);
        alert.severity = static_cast<AlertSeverity>(i % 3);
        alert.title = "Alert Title " + std::to_string(i);
        alert.message = "Detailed message for alert number " + std::to_string(i);
        alert.timestamp = std::chrono::system_clock::now();
        alert.acknowledged = (i % 5 == 0);
        repo.insertAlert(alert);
    }

    AlertFilter emptyFilter;
    BENCHMARK("getAlertsFiltered (empty filter)") {
        return repo.getAlertsFiltered(emptyFilter, 100);
    };

    AlertFilter severityFilter;
    severityFilter.severity = AlertSeverity::Critical;
    BENCHMARK("getAlertsFiltered (severity only)") {
        return repo.getAlertsFiltered(severityFilter, 100);
    };

    AlertFilter typeFilter;
    typeFilter.type = AlertType::HighLatency;
    BENCHMARK("getAlertsFiltered (type only)") {
        return repo.getAlertsFiltered(typeFilter, 100);
    };

    AlertFilter ackFilter;
    ackFilter.acknowledged = false;
    BENCHMARK("getAlertsFiltered (acknowledged only)") {
        return repo.getAlertsFiltered(ackFilter, 100);
    };

    AlertFilter searchFilter;
    searchFilter.searchText = "alert";
    BENCHMARK("getAlertsFiltered (search text)") {
        return repo.getAlertsFiltered(searchFilter, 100);
    };

    AlertFilter combinedFilter;
    combinedFilter.severity = AlertSeverity::Warning;
    combinedFilter.type = AlertType::HighLatency;
    combinedFilter.acknowledged = false;
    BENCHMARK("getAlertsFiltered (combined filters)") {
        return repo.getAlertsFiltered(combinedFilter, 100);
    };
}

// =============================================================================
// Port Scan Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository port scan benchmarks", "[benchmark][MetricsRepository][portscan]") {
    BenchmarkDatabase testDb;
    MetricsRepository repo(testDb.get());

    BENCHMARK("Insert single port scan result") {
        PortScanResult result = createBenchPortScanResult("192.168.1.1", 80);
        return repo.insertPortScanResult(result);
    };

    BENCHMARK("Insert port scan batch (10 ports)") {
        std::vector<PortScanResult> results;
        results.reserve(10);
        for (uint16_t port = 1; port <= 10; ++port) {
            results.push_back(createBenchPortScanResult("192.168.1.100", port));
        }
        repo.insertPortScanResults(results);
        return results.size();
    };

    BENCHMARK("Insert port scan batch (100 ports)") {
        std::vector<PortScanResult> results;
        results.reserve(100);
        for (uint16_t port = 1; port <= 100; ++port) {
            results.push_back(createBenchPortScanResult("192.168.1.200", port));
        }
        repo.insertPortScanResults(results);
        return results.size();
    };

    // Pre-populate for read benchmarks
    std::vector<PortScanResult> scanResults;
    scanResults.reserve(500);
    for (uint16_t port = 1; port <= 500; ++port) {
        scanResults.push_back(createBenchPortScanResult(
            "10.0.0.1", port, static_cast<PortState>(port % 4)));
    }
    repo.insertPortScanResults(scanResults);

    BENCHMARK("getPortScanResults limit 50") {
        return repo.getPortScanResults("10.0.0.1", 50);
    };

    BENCHMARK("getPortScanResults limit 200") {
        return repo.getPortScanResults("10.0.0.1", 200);
    };

    BENCHMARK("getPortScanResults limit 500") {
        return repo.getPortScanResults("10.0.0.1", 500);
    };
}

// =============================================================================
// Export Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository export benchmarks", "[benchmark][MetricsRepository][export]") {
    BenchmarkDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createBenchHost("Export Host", "192.168.1.1"));

    // Pre-populate with data for export
    for (int i = 0; i < 200; ++i) {
        repo.insertPingResult(createBenchPingResult(
            hostId, (i % 8 != 0), std::chrono::microseconds{2000 + i * 50}));
    }

    BENCHMARK("exportToJson (200 results)") {
        return repo.exportToJson(hostId);
    };

    BENCHMARK("exportToCsv (200 results)") {
        return repo.exportToCsv(hostId);
    };

    // Add more data for larger export test
    for (int i = 0; i < 800; ++i) {
        repo.insertPingResult(createBenchPingResult(
            hostId, (i % 10 != 0), std::chrono::microseconds{3000 + i * 30}));
    }

    BENCHMARK("exportToJson (1000 results)") {
        return repo.exportToJson(hostId);
    };

    BENCHMARK("exportToCsv (1000 results)") {
        return repo.exportToCsv(hostId);
    };
}

// =============================================================================
// Cleanup Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository cleanup benchmarks", "[benchmark][MetricsRepository][cleanup]") {
    BenchmarkDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createBenchHost("Cleanup Host", "192.168.1.1"));

    // This benchmark measures cleanup of old data
    // We insert data then clean it up
    BENCHMARK_ADVANCED("cleanupOldPingResults (empty)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&repo] {
            repo.cleanupOldPingResults(std::chrono::hours{24 * 30});
            return 0;
        });
    };

    // Pre-populate then cleanup
    BENCHMARK_ADVANCED("cleanupOldPingResults (with data)")(Catch::Benchmark::Chronometer meter) {
        // Setup: insert data before each measurement
        for (int i = 0; i < 100; ++i) {
            repo.insertPingResult(createBenchPingResult(hostId));
        }
        meter.measure([&repo] {
            repo.cleanupOldPingResults(std::chrono::hours{-1}); // Clean all
            return 0;
        });
    };

    BENCHMARK_ADVANCED("cleanupOldAlerts (with data)")(Catch::Benchmark::Chronometer meter) {
        for (int i = 0; i < 50; ++i) {
            repo.insertAlert(createBenchAlert(hostId));
        }
        meter.measure([&repo] {
            repo.cleanupOldAlerts(std::chrono::hours{-1});
            return 0;
        });
    };
}

// =============================================================================
// Transaction Benchmarks
// =============================================================================

TEST_CASE("MetricsRepository transaction benchmarks", "[benchmark][MetricsRepository][transaction]") {
    BenchmarkDatabase testDb;
    HostRepository hostRepo(testDb.get());
    MetricsRepository repo(testDb.get());

    int64_t hostId = hostRepo.insert(createBenchHost("Transaction Host", "192.168.1.1"));

    BENCHMARK("Insert 10 ping results without transaction") {
        for (int i = 0; i < 10; ++i) {
            repo.insertPingResult(createBenchPingResult(hostId));
        }
        return 10;
    };

    BENCHMARK("Insert 10 ping results with transaction") {
        testDb.get()->transaction([&] {
            for (int i = 0; i < 10; ++i) {
                repo.insertPingResult(createBenchPingResult(hostId));
            }
        });
        return 10;
    };

    BENCHMARK("Insert 50 ping results without transaction") {
        for (int i = 0; i < 50; ++i) {
            repo.insertPingResult(createBenchPingResult(hostId));
        }
        return 50;
    };

    BENCHMARK("Insert 50 ping results with transaction") {
        testDb.get()->transaction([&] {
            for (int i = 0; i < 50; ++i) {
                repo.insertPingResult(createBenchPingResult(hostId));
            }
        });
        return 50;
    };
}
