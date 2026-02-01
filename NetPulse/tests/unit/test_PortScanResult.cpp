#include <catch2/catch_test_macros.hpp>

#include "core/types/PortScanResult.hpp"

using namespace netpulse::core;

TEST_CASE("PortScanResult default values", "[PortScanResult]") {
    PortScanResult result;

    REQUIRE(result.id == 0);
    REQUIRE(result.targetAddress.empty());
    REQUIRE(result.port == 0);
    REQUIRE(result.state == PortState::Unknown);
    REQUIRE(result.serviceName.empty());
}

TEST_CASE("PortScanResult state string conversion", "[PortScanResult]") {
    SECTION("stateToString instance method") {
        PortScanResult result;

        result.state = PortState::Unknown;
        REQUIRE(result.stateToString() == "Unknown");

        result.state = PortState::Open;
        REQUIRE(result.stateToString() == "Open");

        result.state = PortState::Closed;
        REQUIRE(result.stateToString() == "Closed");

        result.state = PortState::Filtered;
        REQUIRE(result.stateToString() == "Filtered");
    }

    SECTION("portStateToString static method") {
        REQUIRE(PortScanResult::portStateToString(PortState::Unknown) == "Unknown");
        REQUIRE(PortScanResult::portStateToString(PortState::Open) == "Open");
        REQUIRE(PortScanResult::portStateToString(PortState::Closed) == "Closed");
        REQUIRE(PortScanResult::portStateToString(PortState::Filtered) == "Filtered");
    }

    SECTION("stateFromString") {
        REQUIRE(PortScanResult::stateFromString("Unknown") == PortState::Unknown);
        REQUIRE(PortScanResult::stateFromString("Open") == PortState::Open);
        REQUIRE(PortScanResult::stateFromString("Closed") == PortState::Closed);
        REQUIRE(PortScanResult::stateFromString("Filtered") == PortState::Filtered);
        REQUIRE(PortScanResult::stateFromString("Invalid") == PortState::Unknown);
    }
}

TEST_CASE("PortScanResult equality", "[PortScanResult]") {
    PortScanResult result1;
    result1.id = 1;
    result1.targetAddress = "192.168.1.1";
    result1.port = 80;
    result1.state = PortState::Open;
    result1.serviceName = "http";
    result1.scanTimestamp = std::chrono::system_clock::now();

    PortScanResult result2 = result1;
    REQUIRE(result1 == result2);

    result2.port = 443;
    REQUIRE_FALSE(result1 == result2);
}

TEST_CASE("PortScanConfig default values", "[PortScanConfig]") {
    PortScanConfig config;

    REQUIRE(config.targetAddress.empty());
    REQUIRE(config.range == PortRange::Common);
    REQUIRE(config.customPorts.empty());
    REQUIRE(config.maxConcurrency == 100);
    REQUIRE(config.timeout == std::chrono::milliseconds{1000});
}

TEST_CASE("PortScanConfig getPortsToScan", "[PortScanConfig]") {
    PortScanConfig config;

    SECTION("Common ports") {
        config.range = PortRange::Common;
        auto ports = config.getPortsToScan();
        REQUIRE_FALSE(ports.empty());
        REQUIRE(std::find(ports.begin(), ports.end(), 22) != ports.end());
        REQUIRE(std::find(ports.begin(), ports.end(), 80) != ports.end());
        REQUIRE(std::find(ports.begin(), ports.end(), 443) != ports.end());
    }

    SECTION("Web ports") {
        config.range = PortRange::Web;
        auto ports = config.getPortsToScan();
        REQUIRE_FALSE(ports.empty());
        REQUIRE(std::find(ports.begin(), ports.end(), 80) != ports.end());
        REQUIRE(std::find(ports.begin(), ports.end(), 443) != ports.end());
        REQUIRE(std::find(ports.begin(), ports.end(), 8080) != ports.end());
    }

    SECTION("Database ports") {
        config.range = PortRange::Database;
        auto ports = config.getPortsToScan();
        REQUIRE_FALSE(ports.empty());
        REQUIRE(std::find(ports.begin(), ports.end(), 3306) != ports.end());
        REQUIRE(std::find(ports.begin(), ports.end(), 5432) != ports.end());
        REQUIRE(std::find(ports.begin(), ports.end(), 27017) != ports.end());
    }

    SECTION("All ports") {
        config.range = PortRange::All;
        auto ports = config.getPortsToScan();
        REQUIRE(ports.size() == 65535);
        REQUIRE(ports.front() == 1);
        REQUIRE(ports.back() == 65535);
    }

    SECTION("Custom ports") {
        config.range = PortRange::Custom;
        config.customPorts = {22, 80, 443, 8080};
        auto ports = config.getPortsToScan();
        REQUIRE(ports.size() == 4);
        REQUIRE(ports == std::vector<uint16_t>{22, 80, 443, 8080});
    }

    SECTION("Empty custom ports") {
        config.range = PortRange::Custom;
        config.customPorts = {};
        auto ports = config.getPortsToScan();
        REQUIRE(ports.empty());
    }
}

TEST_CASE("ServiceDetector detectService", "[ServiceDetector]") {
    SECTION("Known ports") {
        REQUIRE(ServiceDetector::detectService(22) == "ssh");
        REQUIRE(ServiceDetector::detectService(80) == "http");
        REQUIRE(ServiceDetector::detectService(443) == "https");
        REQUIRE(ServiceDetector::detectService(3306) == "mysql");
        REQUIRE(ServiceDetector::detectService(5432) == "postgres");
        REQUIRE(ServiceDetector::detectService(27017) == "mongodb");
        REQUIRE(ServiceDetector::detectService(6379) == "redis");
    }

    SECTION("Unknown port") {
        REQUIRE(ServiceDetector::detectService(12345) == "");
        REQUIRE(ServiceDetector::detectService(54321) == "");
    }
}

TEST_CASE("ServiceDetector getKnownServices", "[ServiceDetector]") {
    const auto& services = ServiceDetector::getKnownServices();

    REQUIRE_FALSE(services.empty());
    REQUIRE(services.at(22) == "ssh");
    REQUIRE(services.at(80) == "http");
    REQUIRE(services.at(443) == "https");
}
