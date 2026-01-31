#include <catch2/catch_test_macros.hpp>

#include "ui/widgets/dashboard/DashboardWidget.hpp"

using namespace netpulse::ui;

TEST_CASE("WidgetType string conversion", "[dashboard][widget]") {
    SECTION("widgetTypeToString returns correct strings") {
        CHECK(widgetTypeToString(WidgetType::Statistics) == "Statistics");
        CHECK(widgetTypeToString(WidgetType::HostStatus) == "HostStatus");
        CHECK(widgetTypeToString(WidgetType::Alerts) == "Alerts");
        CHECK(widgetTypeToString(WidgetType::NetworkOverview) == "NetworkOverview");
        CHECK(widgetTypeToString(WidgetType::LatencyHistory) == "LatencyHistory");
    }

    SECTION("widgetTypeFromString returns correct types") {
        CHECK(widgetTypeFromString("Statistics") == WidgetType::Statistics);
        CHECK(widgetTypeFromString("HostStatus") == WidgetType::HostStatus);
        CHECK(widgetTypeFromString("Alerts") == WidgetType::Alerts);
        CHECK(widgetTypeFromString("NetworkOverview") == WidgetType::NetworkOverview);
        CHECK(widgetTypeFromString("LatencyHistory") == WidgetType::LatencyHistory);
    }

    SECTION("widgetTypeFromString defaults to Statistics for unknown") {
        CHECK(widgetTypeFromString("Unknown") == WidgetType::Statistics);
        CHECK(widgetTypeFromString("") == WidgetType::Statistics);
        CHECK(widgetTypeFromString("InvalidType") == WidgetType::Statistics);
    }
}

TEST_CASE("WidgetConfig JSON serialization", "[dashboard][config]") {
    SECTION("toJson creates correct JSON") {
        WidgetConfig config;
        config.type = WidgetType::Statistics;
        config.title = "Test Statistics";
        config.row = 1;
        config.col = 2;
        config.rowSpan = 2;
        config.colSpan = 3;
        config.settings = {{"key", "value"}};

        auto json = config.toJson();

        CHECK(json["type"] == "Statistics");
        CHECK(json["title"] == "Test Statistics");
        CHECK(json["row"] == 1);
        CHECK(json["col"] == 2);
        CHECK(json["rowSpan"] == 2);
        CHECK(json["colSpan"] == 3);
        CHECK(json["settings"]["key"] == "value");
    }

    SECTION("fromJson parses correctly") {
        nlohmann::json json = {
            {"type", "Alerts"},
            {"title", "My Alerts"},
            {"row", 3},
            {"col", 1},
            {"rowSpan", 1},
            {"colSpan", 2},
            {"settings", {{"maxAlerts", 5}}}
        };

        auto config = WidgetConfig::fromJson(json);

        CHECK(config.type == WidgetType::Alerts);
        CHECK(config.title == "My Alerts");
        CHECK(config.row == 3);
        CHECK(config.col == 1);
        CHECK(config.rowSpan == 1);
        CHECK(config.colSpan == 2);
        CHECK(config.settings["maxAlerts"] == 5);
    }

    SECTION("fromJson uses defaults for missing fields") {
        nlohmann::json json = {};

        auto config = WidgetConfig::fromJson(json);

        CHECK(config.type == WidgetType::Statistics);
        CHECK(config.title == "Widget");
        CHECK(config.row == 0);
        CHECK(config.col == 0);
        CHECK(config.rowSpan == 1);
        CHECK(config.colSpan == 1);
    }

    SECTION("roundtrip serialization preserves data") {
        WidgetConfig original;
        original.type = WidgetType::HostStatus;
        original.title = "Host Monitor";
        original.row = 5;
        original.col = 4;
        original.rowSpan = 3;
        original.colSpan = 2;
        original.settings = {{"showOnlyDown", true}};

        auto json = original.toJson();
        auto restored = WidgetConfig::fromJson(json);

        CHECK(restored.type == original.type);
        CHECK(restored.title == original.title);
        CHECK(restored.row == original.row);
        CHECK(restored.col == original.col);
        CHECK(restored.rowSpan == original.rowSpan);
        CHECK(restored.colSpan == original.colSpan);
        CHECK(restored.settings == original.settings);
    }
}
