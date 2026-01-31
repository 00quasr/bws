#include <catch2/catch_test_macros.hpp>

#include "core/types/PingResult.hpp"
#include "infrastructure/network/AsioContext.hpp"
#include "infrastructure/network/PingService.hpp"

#include <chrono>
#include <thread>

using namespace netpulse::core;
using namespace netpulse::infra;

TEST_CASE("PingResult structure", "[PingService]") {
    SECTION("Default values") {
        PingResult result;
        REQUIRE(result.id == 0);
        REQUIRE(result.hostId == 0);
        REQUIRE(result.success == false);
        REQUIRE(result.latency.count() == 0);
        REQUIRE_FALSE(result.ttl.has_value());
        REQUIRE(result.errorMessage.empty());
    }

    SECTION("Latency conversion") {
        PingResult result;
        result.latency = std::chrono::microseconds(1500);
        REQUIRE(result.latencyMs() == 1.5);

        result.latency = std::chrono::microseconds(10000);
        REQUIRE(result.latencyMs() == 10.0);
    }

    SECTION("Equality comparison") {
        PingResult result1;
        result1.success = true;
        result1.latency = std::chrono::microseconds(1000);
        result1.ttl = 64;

        PingResult result2 = result1;
        REQUIRE(result1 == result2);

        result2.latency = std::chrono::microseconds(2000);
        REQUIRE_FALSE(result1 == result2);
    }
}

TEST_CASE("PingStatistics structure", "[PingService]") {
    SECTION("Default values") {
        PingStatistics stats;
        REQUIRE(stats.hostId == 0);
        REQUIRE(stats.totalPings == 0);
        REQUIRE(stats.successfulPings == 0);
        REQUIRE(stats.packetLossPercent == 0.0);
    }

    SECTION("Success rate calculation") {
        PingStatistics stats;

        // Zero pings should return 0%
        REQUIRE(stats.successRate() == 0.0);

        // 10 pings, 8 successful = 80%
        stats.totalPings = 10;
        stats.successfulPings = 8;
        REQUIRE(stats.successRate() == 80.0);

        // All successful
        stats.successfulPings = 10;
        REQUIRE(stats.successRate() == 100.0);

        // None successful
        stats.successfulPings = 0;
        REQUIRE(stats.successRate() == 0.0);
    }
}

TEST_CASE("PingService initialization", "[PingService]") {
    AsioContext context;
    context.start();

    SECTION("Service can be created") {
        REQUIRE_NOTHROW(PingService(context));
    }

    SECTION("Service initializes correctly") {
        PingService service(context);
        // Should not be monitoring any hosts initially
        REQUIRE_FALSE(service.isMonitoring(1));
        REQUIRE_FALSE(service.isMonitoring(0));
    }

    context.stop();
}

TEST_CASE("PingService monitoring management", "[PingService]") {
    AsioContext context;
    context.start();
    PingService service(context);

    SECTION("Start and stop monitoring") {
        Host host;
        host.id = 1;
        host.name = "Test Host";
        host.address = "127.0.0.1";
        host.pingIntervalSeconds = 60;
        host.warningThresholdMs = 100;
        host.criticalThresholdMs = 500;

        bool callbackInvoked = false;
        service.startMonitoring(host, [&callbackInvoked](const PingResult& /*result*/) {
            callbackInvoked = true;
        });

        REQUIRE(service.isMonitoring(1));

        service.stopMonitoring(1);
        REQUIRE_FALSE(service.isMonitoring(1));
    }

    SECTION("Stop monitoring non-existent host") {
        // Should not throw
        REQUIRE_NOTHROW(service.stopMonitoring(999));
    }

    SECTION("Stop all monitoring") {
        Host host1;
        host1.id = 1;
        host1.name = "Host 1";
        host1.address = "127.0.0.1";
        host1.pingIntervalSeconds = 60;
        host1.warningThresholdMs = 100;
        host1.criticalThresholdMs = 500;

        Host host2;
        host2.id = 2;
        host2.name = "Host 2";
        host2.address = "127.0.0.2";
        host2.pingIntervalSeconds = 60;
        host2.warningThresholdMs = 100;
        host2.criticalThresholdMs = 500;

        service.startMonitoring(host1, [](const PingResult&) {});
        service.startMonitoring(host2, [](const PingResult&) {});

        REQUIRE(service.isMonitoring(1));
        REQUIRE(service.isMonitoring(2));

        service.stopAllMonitoring();

        REQUIRE_FALSE(service.isMonitoring(1));
        REQUIRE_FALSE(service.isMonitoring(2));
    }

    SECTION("Replace monitoring for same host") {
        Host host;
        host.id = 1;
        host.name = "Test Host";
        host.address = "127.0.0.1";
        host.pingIntervalSeconds = 60;
        host.warningThresholdMs = 100;
        host.criticalThresholdMs = 500;

        int callback1Count = 0;
        int callback2Count = 0;

        service.startMonitoring(host, [&callback1Count](const PingResult&) {
            callback1Count++;
        });

        // Start monitoring same host with different callback
        service.startMonitoring(host, [&callback2Count](const PingResult&) {
            callback2Count++;
        });

        // Should still be monitoring
        REQUIRE(service.isMonitoring(1));
    }

    context.stop();
}

TEST_CASE("PingService async ping", "[PingService][integration]") {
    AsioContext context;
    context.start();
    PingService service(context);

    SECTION("Ping localhost returns result") {
        auto future = service.pingAsync("127.0.0.1", std::chrono::milliseconds(5000));

        // Wait for result with timeout
        auto status = future.wait_for(std::chrono::seconds(10));
        REQUIRE(status == std::future_status::ready);

        auto result = future.get();
        REQUIRE(result.timestamp.time_since_epoch().count() > 0);

        // Note: Result may or may not be successful depending on system configuration
        // (e.g., firewall settings, ICMP enabled, privileges)
        // We just verify the operation completes and returns a valid result structure
    }

    SECTION("Ping invalid address handles error gracefully") {
        auto future = service.pingAsync("999.999.999.999", std::chrono::milliseconds(1000));

        auto status = future.wait_for(std::chrono::seconds(5));
        REQUIRE(status == std::future_status::ready);

        auto result = future.get();
        // Should fail with an error message
        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errorMessage.empty());
    }

    SECTION("Ping with short timeout") {
        // Use a non-routable address to trigger timeout
        auto future = service.pingAsync("10.255.255.1", std::chrono::milliseconds(100));

        auto status = future.wait_for(std::chrono::seconds(5));
        REQUIRE(status == std::future_status::ready);

        auto result = future.get();
        // Should typically timeout or fail
        // We just verify the operation completes
        REQUIRE(result.timestamp.time_since_epoch().count() > 0);
    }

    context.stop();
}

TEST_CASE("PingService hostname resolution", "[PingService][integration]") {
    AsioContext context;
    context.start();
    PingService service(context);

    SECTION("Ping with hostname") {
        auto future = service.pingAsync("localhost", std::chrono::milliseconds(5000));

        auto status = future.wait_for(std::chrono::seconds(10));
        REQUIRE(status == std::future_status::ready);

        auto result = future.get();
        REQUIRE(result.timestamp.time_since_epoch().count() > 0);
    }

    context.stop();
}
