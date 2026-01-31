#include <catch2/catch_test_macros.hpp>

#include "core/types/Host.hpp"

using namespace netpulse::core;

TEST_CASE("Host validation", "[Host]") {
    SECTION("Valid host") {
        Host host;
        host.name = "Test Host";
        host.address = "192.168.1.1";
        host.pingIntervalSeconds = 30;
        host.warningThresholdMs = 100;
        host.criticalThresholdMs = 500;

        REQUIRE(host.isValid());
    }

    SECTION("Invalid host - empty name") {
        Host host;
        host.name = "";
        host.address = "192.168.1.1";
        host.pingIntervalSeconds = 30;
        host.warningThresholdMs = 100;
        host.criticalThresholdMs = 500;

        REQUIRE_FALSE(host.isValid());
    }

    SECTION("Invalid host - empty address") {
        Host host;
        host.name = "Test Host";
        host.address = "";
        host.pingIntervalSeconds = 30;
        host.warningThresholdMs = 100;
        host.criticalThresholdMs = 500;

        REQUIRE_FALSE(host.isValid());
    }

    SECTION("Invalid host - zero ping interval") {
        Host host;
        host.name = "Test Host";
        host.address = "192.168.1.1";
        host.pingIntervalSeconds = 0;
        host.warningThresholdMs = 100;
        host.criticalThresholdMs = 500;

        REQUIRE_FALSE(host.isValid());
    }

    SECTION("Invalid host - warning >= critical") {
        Host host;
        host.name = "Test Host";
        host.address = "192.168.1.1";
        host.pingIntervalSeconds = 30;
        host.warningThresholdMs = 500;
        host.criticalThresholdMs = 100;

        REQUIRE_FALSE(host.isValid());
    }
}

TEST_CASE("Host status conversion", "[Host]") {
    SECTION("Status to string") {
        Host host;

        host.status = HostStatus::Unknown;
        REQUIRE(host.statusToString() == "Unknown");

        host.status = HostStatus::Up;
        REQUIRE(host.statusToString() == "Up");

        host.status = HostStatus::Warning;
        REQUIRE(host.statusToString() == "Warning");

        host.status = HostStatus::Down;
        REQUIRE(host.statusToString() == "Down");
    }

    SECTION("String to status") {
        REQUIRE(Host::statusFromString("Unknown") == HostStatus::Unknown);
        REQUIRE(Host::statusFromString("Up") == HostStatus::Up);
        REQUIRE(Host::statusFromString("Warning") == HostStatus::Warning);
        REQUIRE(Host::statusFromString("Down") == HostStatus::Down);
        REQUIRE(Host::statusFromString("invalid") == HostStatus::Unknown);
    }
}

TEST_CASE("Host equality", "[Host]") {
    Host host1;
    host1.id = 1;
    host1.name = "Test";
    host1.address = "192.168.1.1";

    Host host2 = host1;

    REQUIRE(host1 == host2);

    host2.name = "Different";
    REQUIRE_FALSE(host1 == host2);
}
