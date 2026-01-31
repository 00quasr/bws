#include <catch2/catch_test_macros.hpp>

#include "core/types/Host.hpp"
#include "ui/widgets/noc/NocHostCard.hpp"

#include <QApplication>
#include <cstdlib>

using namespace netpulse::ui;
using namespace netpulse::core;

namespace {

bool ensureQApplication() {
    static int argc = 1;
    static char* argv[] = {const_cast<char*>("test")};
    static QApplication* app = nullptr;
    if (!QApplication::instance()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        app = new QApplication(argc, argv);
    }
    return true;
}

} // namespace

TEST_CASE("NocHostCard status updates", "[noc][widget]") {
    REQUIRE(ensureQApplication());

    Host host;
    host.id = 1;
    host.name = "TestHost";
    host.address = "192.168.1.1";
    host.status = HostStatus::Unknown;
    host.enabled = true;

    NocHostCard card(host);

    SECTION("Card stores correct host ID") {
        CHECK(card.hostId() == 1);
    }

    SECTION("Card updates to Up status") {
        card.updateStatus(HostStatus::Up, 15.5);
        CHECK(card.property("statusClass").toString() == "up");
    }

    SECTION("Card updates to Down status") {
        card.updateStatus(HostStatus::Down, 0.0);
        CHECK(card.property("statusClass").toString() == "down");
    }

    SECTION("Card updates to Warning status") {
        card.updateStatus(HostStatus::Warning, 250.0);
        CHECK(card.property("statusClass").toString() == "warning");
    }

    SECTION("Card updates to Unknown status") {
        card.updateStatus(HostStatus::Unknown, 0.0);
        CHECK(card.property("statusClass").toString() == "unknown");
    }
}

TEST_CASE("NocHostCard host data handling", "[noc][widget]") {
    REQUIRE(ensureQApplication());

    SECTION("Card handles host with empty name") {
        Host host;
        host.id = 42;
        host.name = "";
        host.address = "10.0.0.1";
        host.status = HostStatus::Up;

        NocHostCard card(host);
        CHECK(card.hostId() == 42);
    }

    SECTION("Card handles host with long name") {
        Host host;
        host.id = 99;
        host.name = "VeryLongHostNameThatMightCauseTruncationIssues";
        host.address = "192.168.100.200";
        host.status = HostStatus::Up;

        NocHostCard card(host);
        CHECK(card.hostId() == 99);
    }

    SECTION("Card handles zero latency") {
        Host host;
        host.id = 1;
        host.name = "TestHost";
        host.status = HostStatus::Up;

        NocHostCard card(host);
        card.updateStatus(HostStatus::Up, 0.0);
        CHECK(card.property("statusClass").toString() == "up");
    }

    SECTION("Card handles high latency") {
        Host host;
        host.id = 1;
        host.name = "TestHost";
        host.status = HostStatus::Up;

        NocHostCard card(host);
        card.updateStatus(HostStatus::Up, 9999.9);
        CHECK(card.property("statusClass").toString() == "up");
    }

    SECTION("Multiple status transitions work correctly") {
        Host host;
        host.id = 1;
        host.name = "TestHost";
        host.status = HostStatus::Unknown;

        NocHostCard card(host);

        card.updateStatus(HostStatus::Up, 10.0);
        CHECK(card.property("statusClass").toString() == "up");

        card.updateStatus(HostStatus::Down, 0.0);
        CHECK(card.property("statusClass").toString() == "down");

        card.updateStatus(HostStatus::Warning, 150.0);
        CHECK(card.property("statusClass").toString() == "warning");

        card.updateStatus(HostStatus::Up, 5.0);
        CHECK(card.property("statusClass").toString() == "up");
    }
}
