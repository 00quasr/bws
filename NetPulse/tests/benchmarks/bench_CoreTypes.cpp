#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"
#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"

#include <chrono>
#include <string>
#include <vector>

using namespace netpulse::core;

// =============================================================================
// PingResult Benchmarks
// =============================================================================

TEST_CASE("PingResult benchmarks", "[benchmark][PingResult]") {
    BENCHMARK("PingResult default construction") {
        return PingResult{};
    };

    PingResult result;
    result.latency = std::chrono::microseconds{12345};

    BENCHMARK("PingResult::latencyMs conversion") {
        return result.latencyMs();
    };

    BENCHMARK("PingResult::latencyMs with varying latencies") {
        PingResult r;
        double total = 0.0;
        for (int i = 1; i <= 100; ++i) {
            r.latency = std::chrono::microseconds{i * 1000};
            total += r.latencyMs();
        }
        return total;
    };

    PingResult r1, r2;
    r1.id = 1;
    r1.hostId = 10;
    r1.latency = std::chrono::microseconds{5000};
    r1.success = true;
    r1.ttl = 64;
    r1.timestamp = std::chrono::system_clock::now();
    r2 = r1;

    BENCHMARK("PingResult equality comparison (equal)") {
        return r1 == r2;
    };

    r2.latency = std::chrono::microseconds{6000};

    BENCHMARK("PingResult equality comparison (not equal)") {
        return r1 == r2;
    };

    BENCHMARK("PingResult copy construction") {
        return PingResult{r1};
    };

    BENCHMARK("PingResult move construction") {
        PingResult source = r1;
        return PingResult{std::move(source)};
    };
}

TEST_CASE("PingStatistics benchmarks", "[benchmark][PingResult][Statistics]") {
    BENCHMARK("PingStatistics default construction") {
        return PingStatistics{};
    };

    PingStatistics stats;
    stats.totalPings = 1000;
    stats.successfulPings = 950;

    BENCHMARK("PingStatistics::successRate calculation") {
        return stats.successRate();
    };

    BENCHMARK("PingStatistics::successRate with zero pings") {
        PingStatistics zeroStats;
        zeroStats.totalPings = 0;
        zeroStats.successfulPings = 0;
        return zeroStats.successRate();
    };

    BENCHMARK("PingStatistics::successRate batch calculations") {
        double total = 0.0;
        for (int i = 1; i <= 100; ++i) {
            PingStatistics s;
            s.totalPings = i * 10;
            s.successfulPings = i * 9;
            total += s.successRate();
        }
        return total;
    };
}

// =============================================================================
// Alert Benchmarks
// =============================================================================

TEST_CASE("Alert benchmarks", "[benchmark][Alert]") {
    BENCHMARK("Alert default construction") {
        return Alert{};
    };

    Alert alert;
    alert.id = 1;
    alert.hostId = 10;
    alert.type = AlertType::HighLatency;
    alert.severity = AlertSeverity::Warning;
    alert.title = "Test Alert Title";
    alert.message = "This is a test alert message with some content";
    alert.timestamp = std::chrono::system_clock::now();
    alert.acknowledged = false;

    BENCHMARK("Alert::typeToString") {
        return alert.typeToString();
    };

    BENCHMARK("Alert::severityToString") {
        return alert.severityToString();
    };

    BENCHMARK("Alert::typeToString all types") {
        std::string result;
        Alert a;
        a.type = AlertType::HostDown;
        result += a.typeToString();
        a.type = AlertType::HighLatency;
        result += a.typeToString();
        a.type = AlertType::PacketLoss;
        result += a.typeToString();
        a.type = AlertType::HostRecovered;
        result += a.typeToString();
        a.type = AlertType::ScanComplete;
        result += a.typeToString();
        return result;
    };

    BENCHMARK("Alert::severityToString all severities") {
        std::string result;
        Alert a;
        a.severity = AlertSeverity::Info;
        result += a.severityToString();
        a.severity = AlertSeverity::Warning;
        result += a.severityToString();
        a.severity = AlertSeverity::Critical;
        result += a.severityToString();
        return result;
    };

    BENCHMARK("Alert::typeFromString") {
        return Alert::typeFromString("HighLatency");
    };

    BENCHMARK("Alert::severityFromString") {
        return Alert::severityFromString("Warning");
    };

    BENCHMARK("Alert copy construction") {
        return Alert{alert};
    };

    Alert alert2 = alert;
    BENCHMARK("Alert equality comparison (equal)") {
        return alert == alert2;
    };

    alert2.acknowledged = true;
    BENCHMARK("Alert equality comparison (not equal)") {
        return alert == alert2;
    };
}

TEST_CASE("AlertFilter benchmarks", "[benchmark][Alert][Filter]") {
    BENCHMARK("AlertFilter default construction") {
        return AlertFilter{};
    };

    AlertFilter emptyFilter;

    BENCHMARK("AlertFilter::isEmpty (empty filter)") {
        return emptyFilter.isEmpty();
    };

    AlertFilter populatedFilter;
    populatedFilter.severity = AlertSeverity::Warning;
    populatedFilter.type = AlertType::HighLatency;
    populatedFilter.acknowledged = false;
    populatedFilter.searchText = "test search";

    BENCHMARK("AlertFilter::isEmpty (populated filter)") {
        return populatedFilter.isEmpty();
    };

    BENCHMARK("AlertFilter copy construction") {
        return AlertFilter{populatedFilter};
    };
}

// =============================================================================
// Host Benchmarks
// =============================================================================

TEST_CASE("Host benchmarks", "[benchmark][Host]") {
    BENCHMARK("Host default construction") {
        return Host{};
    };

    Host host;
    host.id = 1;
    host.name = "Test Server";
    host.address = "192.168.1.100";
    host.pingIntervalSeconds = 30;
    host.warningThresholdMs = 100;
    host.criticalThresholdMs = 500;
    host.status = HostStatus::Up;
    host.enabled = true;
    host.groupId = 5;
    host.createdAt = std::chrono::system_clock::now();
    host.lastChecked = std::chrono::system_clock::now();

    BENCHMARK("Host::isValid (valid host)") {
        return host.isValid();
    };

    Host invalidHost;
    invalidHost.name = "";
    invalidHost.address = "";

    BENCHMARK("Host::isValid (invalid host)") {
        return invalidHost.isValid();
    };

    BENCHMARK("Host::statusToString") {
        return host.statusToString();
    };

    BENCHMARK("Host::statusToString all statuses") {
        std::string result;
        Host h;
        h.status = HostStatus::Unknown;
        result += h.statusToString();
        h.status = HostStatus::Up;
        result += h.statusToString();
        h.status = HostStatus::Warning;
        result += h.statusToString();
        h.status = HostStatus::Down;
        result += h.statusToString();
        return result;
    };

    BENCHMARK("Host::statusFromString") {
        return Host::statusFromString("Up");
    };

    BENCHMARK("Host copy construction") {
        return Host{host};
    };

    Host host2 = host;
    BENCHMARK("Host equality comparison (equal)") {
        return host == host2;
    };

    host2.status = HostStatus::Down;
    BENCHMARK("Host equality comparison (not equal)") {
        return host == host2;
    };
}

// =============================================================================
// Batch Operation Benchmarks
// =============================================================================

TEST_CASE("Core types batch operation benchmarks", "[benchmark][batch]") {
    BENCHMARK("Create 100 PingResults") {
        std::vector<PingResult> results;
        results.reserve(100);
        for (int i = 0; i < 100; ++i) {
            PingResult r;
            r.id = i;
            r.hostId = i % 10;
            r.latency = std::chrono::microseconds{5000 + i * 100};
            r.success = (i % 5 != 0);
            r.ttl = 64;
            r.timestamp = std::chrono::system_clock::now();
            results.push_back(r);
        }
        return results.size();
    };

    BENCHMARK("Create 100 Alerts") {
        std::vector<Alert> alerts;
        alerts.reserve(100);
        for (int i = 0; i < 100; ++i) {
            Alert a;
            a.id = i;
            a.hostId = i % 10;
            a.type = static_cast<AlertType>(i % 5);
            a.severity = static_cast<AlertSeverity>(i % 3);
            a.title = "Alert " + std::to_string(i);
            a.message = "Message for alert " + std::to_string(i);
            a.timestamp = std::chrono::system_clock::now();
            a.acknowledged = (i % 2 == 0);
            alerts.push_back(a);
        }
        return alerts.size();
    };

    BENCHMARK("Create 100 Hosts") {
        std::vector<Host> hosts;
        hosts.reserve(100);
        for (int i = 0; i < 100; ++i) {
            Host h;
            h.id = i;
            h.name = "Host " + std::to_string(i);
            h.address = "192.168.1." + std::to_string(i % 256);
            h.pingIntervalSeconds = 30;
            h.warningThresholdMs = 100;
            h.criticalThresholdMs = 500;
            h.status = static_cast<HostStatus>(i % 4);
            h.enabled = (i % 3 != 0);
            h.createdAt = std::chrono::system_clock::now();
            hosts.push_back(h);
        }
        return hosts.size();
    };

    std::vector<PingResult> pingResults;
    pingResults.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        PingResult r;
        r.latency = std::chrono::microseconds{1000 + i};
        pingResults.push_back(r);
    }

    BENCHMARK("Batch latencyMs conversion (1000 results)") {
        double total = 0.0;
        for (const auto& r : pingResults) {
            total += r.latencyMs();
        }
        return total;
    };

    std::vector<Host> hosts;
    hosts.reserve(100);
    for (int i = 0; i < 100; ++i) {
        Host h;
        h.name = "Host " + std::to_string(i);
        h.address = "192.168.1." + std::to_string(i);
        hosts.push_back(h);
    }

    BENCHMARK("Batch isValid check (100 hosts)") {
        int validCount = 0;
        for (const auto& h : hosts) {
            if (h.isValid()) {
                ++validCount;
            }
        }
        return validCount;
    };
}
