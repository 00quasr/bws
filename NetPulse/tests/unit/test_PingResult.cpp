#include <catch2/catch_test_macros.hpp>

#include "core/types/PingResult.hpp"

using namespace netpulse::core;

TEST_CASE("PingResult default values", "[PingResult]") {
    PingResult result;

    REQUIRE(result.id == 0);
    REQUIRE(result.hostId == 0);
    REQUIRE(result.latency == std::chrono::microseconds{0});
    REQUIRE(result.success == false);
    REQUIRE_FALSE(result.ttl.has_value());
    REQUIRE(result.errorMessage.empty());
}

TEST_CASE("PingResult latencyMs conversion", "[PingResult]") {
    PingResult result;

    SECTION("Zero latency") {
        result.latency = std::chrono::microseconds{0};
        REQUIRE(result.latencyMs() == 0.0);
    }

    SECTION("Small latency") {
        result.latency = std::chrono::microseconds{500};
        REQUIRE(result.latencyMs() == 0.5);
    }

    SECTION("One millisecond") {
        result.latency = std::chrono::microseconds{1000};
        REQUIRE(result.latencyMs() == 1.0);
    }

    SECTION("Large latency") {
        result.latency = std::chrono::microseconds{150000};
        REQUIRE(result.latencyMs() == 150.0);
    }

    SECTION("Fractional milliseconds") {
        result.latency = std::chrono::microseconds{1234};
        REQUIRE(result.latencyMs() == 1.234);
    }
}

TEST_CASE("PingResult equality", "[PingResult]") {
    PingResult result1;
    result1.id = 1;
    result1.hostId = 10;
    result1.latency = std::chrono::microseconds{5000};
    result1.success = true;
    result1.ttl = 64;
    result1.timestamp = std::chrono::system_clock::now();

    PingResult result2 = result1;
    REQUIRE(result1 == result2);

    result2.success = false;
    REQUIRE_FALSE(result1 == result2);
}

TEST_CASE("PingResult with TTL", "[PingResult]") {
    PingResult result;

    SECTION("No TTL") {
        REQUIRE_FALSE(result.ttl.has_value());
    }

    SECTION("With TTL") {
        result.ttl = 64;
        REQUIRE(result.ttl.has_value());
        REQUIRE(result.ttl.value() == 64);
    }
}

TEST_CASE("PingStatistics default values", "[PingResult][Statistics]") {
    PingStatistics stats;

    REQUIRE(stats.hostId == 0);
    REQUIRE(stats.totalPings == 0);
    REQUIRE(stats.successfulPings == 0);
    REQUIRE(stats.minLatency == std::chrono::microseconds{0});
    REQUIRE(stats.maxLatency == std::chrono::microseconds{0});
    REQUIRE(stats.avgLatency == std::chrono::microseconds{0});
    REQUIRE(stats.jitter == std::chrono::microseconds{0});
    REQUIRE(stats.packetLossPercent == 0.0);
}

TEST_CASE("PingStatistics successRate", "[PingResult][Statistics]") {
    PingStatistics stats;

    SECTION("Zero pings") {
        stats.totalPings = 0;
        stats.successfulPings = 0;
        REQUIRE(stats.successRate() == 0.0);
    }

    SECTION("100% success rate") {
        stats.totalPings = 100;
        stats.successfulPings = 100;
        REQUIRE(stats.successRate() == 100.0);
    }

    SECTION("50% success rate") {
        stats.totalPings = 100;
        stats.successfulPings = 50;
        REQUIRE(stats.successRate() == 50.0);
    }

    SECTION("0% success rate") {
        stats.totalPings = 100;
        stats.successfulPings = 0;
        REQUIRE(stats.successRate() == 0.0);
    }

    SECTION("Partial success rate") {
        stats.totalPings = 10;
        stats.successfulPings = 7;
        REQUIRE(stats.successRate() == 70.0);
    }
}
