#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"

using namespace netpulse::core;

TEST_CASE("Alert type string conversion", "[Alert]") {
    SECTION("typeToString") {
        Alert alert;

        alert.type = AlertType::HostDown;
        REQUIRE(alert.typeToString() == "HostDown");

        alert.type = AlertType::HighLatency;
        REQUIRE(alert.typeToString() == "HighLatency");

        alert.type = AlertType::PacketLoss;
        REQUIRE(alert.typeToString() == "PacketLoss");

        alert.type = AlertType::HostRecovered;
        REQUIRE(alert.typeToString() == "HostRecovered");

        alert.type = AlertType::ScanComplete;
        REQUIRE(alert.typeToString() == "ScanComplete");
    }

    SECTION("typeFromString") {
        REQUIRE(Alert::typeFromString("HostDown") == AlertType::HostDown);
        REQUIRE(Alert::typeFromString("HighLatency") == AlertType::HighLatency);
        REQUIRE(Alert::typeFromString("PacketLoss") == AlertType::PacketLoss);
        REQUIRE(Alert::typeFromString("HostRecovered") == AlertType::HostRecovered);
        REQUIRE(Alert::typeFromString("ScanComplete") == AlertType::ScanComplete);
        REQUIRE(Alert::typeFromString("Invalid") == AlertType::HostDown);
    }
}

TEST_CASE("Alert severity string conversion", "[Alert]") {
    SECTION("severityToString") {
        Alert alert;

        alert.severity = AlertSeverity::Info;
        REQUIRE(alert.severityToString() == "Info");

        alert.severity = AlertSeverity::Warning;
        REQUIRE(alert.severityToString() == "Warning");

        alert.severity = AlertSeverity::Critical;
        REQUIRE(alert.severityToString() == "Critical");
    }

    SECTION("severityFromString") {
        REQUIRE(Alert::severityFromString("Info") == AlertSeverity::Info);
        REQUIRE(Alert::severityFromString("Warning") == AlertSeverity::Warning);
        REQUIRE(Alert::severityFromString("Critical") == AlertSeverity::Critical);
        REQUIRE(Alert::severityFromString("Invalid") == AlertSeverity::Info);
    }
}

TEST_CASE("Alert default values", "[Alert]") {
    Alert alert;

    REQUIRE(alert.id == 0);
    REQUIRE(alert.hostId == 0);
    REQUIRE(alert.type == AlertType::HostDown);
    REQUIRE(alert.severity == AlertSeverity::Info);
    REQUIRE(alert.title.empty());
    REQUIRE(alert.message.empty());
    REQUIRE(alert.acknowledged == false);
}

TEST_CASE("Alert equality", "[Alert]") {
    Alert alert1;
    alert1.id = 1;
    alert1.hostId = 10;
    alert1.type = AlertType::HighLatency;
    alert1.severity = AlertSeverity::Warning;
    alert1.title = "Test Alert";
    alert1.message = "This is a test";
    alert1.acknowledged = true;
    alert1.timestamp = std::chrono::system_clock::now();

    Alert alert2 = alert1;
    REQUIRE(alert1 == alert2);

    alert2.title = "Different";
    REQUIRE_FALSE(alert1 == alert2);
}

TEST_CASE("AlertThresholds default values", "[Alert]") {
    AlertThresholds thresholds;

    REQUIRE(thresholds.latencyWarningMs == 100);
    REQUIRE(thresholds.latencyCriticalMs == 500);
    REQUIRE(thresholds.packetLossWarningPercent == 5.0);
    REQUIRE(thresholds.packetLossCriticalPercent == 20.0);
    REQUIRE(thresholds.consecutiveFailuresForDown == 3);
}

TEST_CASE("AlertFilter isEmpty", "[Alert][Filter]") {
    SECTION("Empty filter") {
        AlertFilter filter;
        REQUIRE(filter.isEmpty());
    }

    SECTION("Filter with severity") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Critical;
        REQUIRE_FALSE(filter.isEmpty());
    }

    SECTION("Filter with type") {
        AlertFilter filter;
        filter.type = AlertType::HostDown;
        REQUIRE_FALSE(filter.isEmpty());
    }

    SECTION("Filter with acknowledged") {
        AlertFilter filter;
        filter.acknowledged = true;
        REQUIRE_FALSE(filter.isEmpty());
    }

    SECTION("Filter with searchText") {
        AlertFilter filter;
        filter.searchText = "test";
        REQUIRE_FALSE(filter.isEmpty());
    }

    SECTION("Filter with multiple fields") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Warning;
        filter.type = AlertType::HighLatency;
        filter.searchText = "latency";
        REQUIRE_FALSE(filter.isEmpty());
    }
}
