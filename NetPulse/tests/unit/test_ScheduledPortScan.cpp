#include <catch2/catch_test_macros.hpp>

#include "core/types/ScheduledPortScan.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/ScheduledScanRepository.hpp"
#include "infrastructure/network/AsioContext.hpp"
#include "infrastructure/network/PortScanner.hpp"
#include "infrastructure/network/ScheduledPortScanner.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

using namespace netpulse::core;
using namespace netpulse::infra;

TEST_CASE("PortChange structure", "[ScheduledPortScan]") {
    SECTION("Default values") {
        PortChange change;
        REQUIRE(change.port == 0);
        REQUIRE(change.changeType == PortChangeType::StateChanged);
        REQUIRE(change.previousState == PortState::Unknown);
        REQUIRE(change.currentState == PortState::Unknown);
        REQUIRE(change.serviceName.empty());
    }

    SECTION("Change type to string conversion") {
        PortChange change;

        change.changeType = PortChangeType::NewOpen;
        REQUIRE(change.changeTypeToString() == "NewOpen");

        change.changeType = PortChangeType::NewClosed;
        REQUIRE(change.changeTypeToString() == "NewClosed");

        change.changeType = PortChangeType::StateChanged;
        REQUIRE(change.changeTypeToString() == "StateChanged");
    }

    SECTION("Change type from string conversion") {
        REQUIRE(PortChange::changeTypeFromString("NewOpen") == PortChangeType::NewOpen);
        REQUIRE(PortChange::changeTypeFromString("NewClosed") == PortChangeType::NewClosed);
        REQUIRE(PortChange::changeTypeFromString("StateChanged") == PortChangeType::StateChanged);
        REQUIRE(PortChange::changeTypeFromString("Invalid") == PortChangeType::StateChanged);
    }

    SECTION("Equality comparison") {
        PortChange change1;
        change1.port = 80;
        change1.changeType = PortChangeType::NewOpen;
        change1.previousState = PortState::Closed;
        change1.currentState = PortState::Open;
        change1.serviceName = "http";

        PortChange change2 = change1;
        REQUIRE(change1 == change2);

        change2.port = 443;
        REQUIRE_FALSE(change1 == change2);
    }
}

TEST_CASE("PortScanDiff structure", "[ScheduledPortScan]") {
    SECTION("Default values") {
        PortScanDiff diff;
        REQUIRE(diff.id == 0);
        REQUIRE(diff.targetAddress.empty());
        REQUIRE(diff.changes.empty());
        REQUIRE(diff.totalPortsScanned == 0);
        REQUIRE(diff.openPortsBefore == 0);
        REQUIRE(diff.openPortsAfter == 0);
    }

    SECTION("Has changes") {
        PortScanDiff diff;
        REQUIRE_FALSE(diff.hasChanges());

        PortChange change;
        change.port = 80;
        diff.changes.push_back(change);
        REQUIRE(diff.hasChanges());
    }

    SECTION("Count new open ports") {
        PortScanDiff diff;

        PortChange open1{.port = 80, .changeType = PortChangeType::NewOpen};
        PortChange open2{.port = 443, .changeType = PortChangeType::NewOpen};
        PortChange closed{.port = 22, .changeType = PortChangeType::NewClosed};
        PortChange changed{.port = 8080, .changeType = PortChangeType::StateChanged};

        diff.changes = {open1, open2, closed, changed};

        REQUIRE(diff.newOpenPorts() == 2);
        REQUIRE(diff.newClosedPorts() == 1);
    }

    SECTION("Equality comparison") {
        PortScanDiff diff1;
        diff1.targetAddress = "192.168.1.1";
        diff1.totalPortsScanned = 100;
        diff1.openPortsBefore = 5;
        diff1.openPortsAfter = 7;

        PortScanDiff diff2 = diff1;
        REQUIRE(diff1 == diff2);

        diff2.openPortsAfter = 10;
        REQUIRE_FALSE(diff1 == diff2);
    }
}

TEST_CASE("ScheduledScanConfig structure", "[ScheduledPortScan]") {
    SECTION("Default values") {
        ScheduledScanConfig config;
        REQUIRE(config.id == 0);
        REQUIRE(config.name.empty());
        REQUIRE(config.targetAddress.empty());
        REQUIRE(config.portRange == PortRange::Common);
        REQUIRE(config.customPorts.empty());
        REQUIRE(config.intervalMinutes == 60);
        REQUIRE(config.enabled == true);
        REQUIRE(config.notifyOnChanges == true);
    }

    SECTION("Validation") {
        ScheduledScanConfig config;
        REQUIRE_FALSE(config.isValid()); // Empty name and address

        config.name = "Test Scan";
        REQUIRE_FALSE(config.isValid()); // Empty address

        config.targetAddress = "192.168.1.1";
        REQUIRE(config.isValid());

        config.intervalMinutes = 0;
        REQUIRE_FALSE(config.isValid()); // Invalid interval

        config.intervalMinutes = 30;
        REQUIRE(config.isValid());

        config.portRange = PortRange::Custom;
        REQUIRE_FALSE(config.isValid()); // Custom range but no ports

        config.customPorts = {80, 443, 8080};
        REQUIRE(config.isValid());
    }

    SECTION("Convert to PortScanConfig") {
        ScheduledScanConfig config;
        config.targetAddress = "192.168.1.1";
        config.portRange = PortRange::Web;
        config.customPorts = {80, 443};

        auto scanConfig = config.toPortScanConfig();
        REQUIRE(scanConfig.targetAddress == "192.168.1.1");
        REQUIRE(scanConfig.range == PortRange::Web);
        REQUIRE(scanConfig.customPorts == std::vector<uint16_t>{80, 443});
    }
}

TEST_CASE("ScheduledPortScanner initialization", "[ScheduledPortScan]") {
    AsioContext context;
    context.start();
    PortScanner portScanner(context);

    SECTION("Service can be created") {
        REQUIRE_NOTHROW(ScheduledPortScanner(context, portScanner));
    }

    SECTION("Service initializes correctly") {
        ScheduledPortScanner scheduler(context, portScanner);
        REQUIRE_FALSE(scheduler.isRunning());
        REQUIRE(scheduler.getSchedules().empty());
    }

    context.stop();
}

TEST_CASE("ScheduledPortScanner schedule management", "[ScheduledPortScan]") {
    AsioContext context;
    context.start();
    PortScanner portScanner(context);
    ScheduledPortScanner scheduler(context, portScanner);

    SECTION("Add schedule") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "127.0.0.1";
        config.intervalMinutes = 5;

        scheduler.addSchedule(config);
        auto schedules = scheduler.getSchedules();
        REQUIRE(schedules.size() == 1);
        REQUIRE(schedules[0].name == "Test Scan");
        REQUIRE(schedules[0].targetAddress == "127.0.0.1");
        REQUIRE(schedules[0].id > 0);
    }

    SECTION("Get schedule by ID") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "127.0.0.1";
        config.intervalMinutes = 5;

        scheduler.addSchedule(config);
        auto schedules = scheduler.getSchedules();
        auto id = schedules[0].id;

        auto retrieved = scheduler.getSchedule(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Test Scan");

        auto notFound = scheduler.getSchedule(99999);
        REQUIRE_FALSE(notFound.has_value());
    }

    SECTION("Update schedule") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "127.0.0.1";
        config.intervalMinutes = 5;

        scheduler.addSchedule(config);
        auto schedules = scheduler.getSchedules();
        auto id = schedules[0].id;

        config.id = id;
        config.name = "Updated Scan";
        config.intervalMinutes = 10;
        scheduler.updateSchedule(config);

        auto retrieved = scheduler.getSchedule(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Updated Scan");
        REQUIRE(retrieved->intervalMinutes == 10);
    }

    SECTION("Remove schedule") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "127.0.0.1";
        config.intervalMinutes = 5;

        scheduler.addSchedule(config);
        auto schedules = scheduler.getSchedules();
        auto id = schedules[0].id;

        scheduler.removeSchedule(id);
        REQUIRE(scheduler.getSchedules().empty());
    }

    SECTION("Enable/disable schedule") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "127.0.0.1";
        config.intervalMinutes = 5;
        config.enabled = true;

        scheduler.addSchedule(config);
        auto schedules = scheduler.getSchedules();
        auto id = schedules[0].id;

        scheduler.enableSchedule(id, false);
        auto retrieved = scheduler.getSchedule(id);
        REQUIRE(retrieved.has_value());
        REQUIRE_FALSE(retrieved->enabled);

        scheduler.enableSchedule(id, true);
        retrieved = scheduler.getSchedule(id);
        REQUIRE(retrieved->enabled);
    }

    context.stop();
}

TEST_CASE("ScheduledPortScanner start/stop", "[ScheduledPortScan]") {
    AsioContext context;
    context.start();
    PortScanner portScanner(context);
    ScheduledPortScanner scheduler(context, portScanner);

    SECTION("Start and stop") {
        REQUIRE_FALSE(scheduler.isRunning());

        scheduler.start();
        REQUIRE(scheduler.isRunning());

        scheduler.stop();
        REQUIRE_FALSE(scheduler.isRunning());
    }

    SECTION("Multiple start calls are idempotent") {
        scheduler.start();
        scheduler.start();
        REQUIRE(scheduler.isRunning());

        scheduler.stop();
        REQUIRE_FALSE(scheduler.isRunning());
    }

    context.stop();
}

TEST_CASE("ScheduledPortScanner diff computation", "[ScheduledPortScan]") {
    AsioContext context;
    context.start();
    PortScanner portScanner(context);
    ScheduledPortScanner scheduler(context, portScanner);

    auto now = std::chrono::system_clock::now();

    SECTION("Empty scans produce no changes") {
        std::vector<PortScanResult> previous;
        std::vector<PortScanResult> current;

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE_FALSE(diff.hasChanges());
        REQUIRE(diff.openPortsBefore == 0);
        REQUIRE(diff.openPortsAfter == 0);
    }

    SECTION("Detect newly opened ports") {
        std::vector<PortScanResult> previous = {
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Closed, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 443, .state = PortState::Closed, .scanTimestamp = now},
        };
        std::vector<PortScanResult> current = {
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Open, .serviceName = "http", .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 443, .state = PortState::Closed, .scanTimestamp = now},
        };

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE(diff.hasChanges());
        REQUIRE(diff.changes.size() == 1);
        REQUIRE(diff.changes[0].port == 80);
        REQUIRE(diff.changes[0].changeType == PortChangeType::NewOpen);
        REQUIRE(diff.openPortsBefore == 0);
        REQUIRE(diff.openPortsAfter == 1);
    }

    SECTION("Detect newly closed ports") {
        std::vector<PortScanResult> previous = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .serviceName = "ssh", .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Open, .serviceName = "http", .scanTimestamp = now},
        };
        std::vector<PortScanResult> current = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .serviceName = "ssh", .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Closed, .scanTimestamp = now},
        };

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE(diff.hasChanges());
        REQUIRE(diff.changes.size() == 1);
        REQUIRE(diff.changes[0].port == 80);
        REQUIRE(diff.changes[0].changeType == PortChangeType::NewClosed);
        REQUIRE(diff.openPortsBefore == 2);
        REQUIRE(diff.openPortsAfter == 1);
    }

    SECTION("Detect multiple changes") {
        std::vector<PortScanResult> previous = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Closed, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 443, .state = PortState::Filtered, .scanTimestamp = now},
        };
        std::vector<PortScanResult> current = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Closed, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Open, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 443, .state = PortState::Open, .scanTimestamp = now},
        };

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE(diff.hasChanges());
        REQUIRE(diff.changes.size() == 3);
        REQUIRE(diff.newOpenPorts() == 2);
        REQUIRE(diff.newClosedPorts() == 1);
        REQUIRE(diff.openPortsBefore == 1);
        REQUIRE(diff.openPortsAfter == 2);
    }

    SECTION("New port in scan detected as NewOpen") {
        std::vector<PortScanResult> previous = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .scanTimestamp = now},
        };
        std::vector<PortScanResult> current = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 8080, .state = PortState::Open, .serviceName = "http-alt", .scanTimestamp = now},
        };

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE(diff.hasChanges());
        REQUIRE(diff.changes.size() == 1);
        REQUIRE(diff.changes[0].port == 8080);
        REQUIRE(diff.changes[0].changeType == PortChangeType::NewOpen);
    }

    SECTION("Port removed from scan detected as NewClosed if was open") {
        std::vector<PortScanResult> previous = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .serviceName = "ssh", .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Open, .serviceName = "http", .scanTimestamp = now},
        };
        std::vector<PortScanResult> current = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .serviceName = "ssh", .scanTimestamp = now},
        };

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE(diff.hasChanges());
        REQUIRE(diff.changes.size() == 1);
        REQUIRE(diff.changes[0].port == 80);
        REQUIRE(diff.changes[0].changeType == PortChangeType::NewClosed);
    }

    SECTION("No changes when states are the same") {
        std::vector<PortScanResult> previous = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Closed, .scanTimestamp = now},
        };
        std::vector<PortScanResult> current = {
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Closed, .scanTimestamp = now},
        };

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE_FALSE(diff.hasChanges());
    }

    SECTION("Changes sorted by port") {
        std::vector<PortScanResult> previous = {
            {.targetAddress = "192.168.1.1", .port = 443, .state = PortState::Closed, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Closed, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Closed, .scanTimestamp = now},
        };
        std::vector<PortScanResult> current = {
            {.targetAddress = "192.168.1.1", .port = 443, .state = PortState::Open, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 22, .state = PortState::Open, .scanTimestamp = now},
            {.targetAddress = "192.168.1.1", .port = 80, .state = PortState::Open, .scanTimestamp = now},
        };

        auto diff = scheduler.computeDiff("192.168.1.1", previous, current);
        REQUIRE(diff.changes.size() == 3);
        REQUIRE(diff.changes[0].port == 22);
        REQUIRE(diff.changes[1].port == 80);
        REQUIRE(diff.changes[2].port == 443);
    }

    context.stop();
}

TEST_CASE("ScheduledScanRepository database operations", "[ScheduledPortScan][Database]") {
    std::string testDbPath = "/tmp/test_scheduled_scan_" +
                              std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                              ".db";

    auto db = std::make_shared<Database>(testDbPath);
    db->runMigrations();
    ScheduledScanRepository repo(db);

    SECTION("Insert and retrieve schedule") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "192.168.1.1";
        config.portRange = PortRange::Web;
        config.intervalMinutes = 30;
        config.enabled = true;
        config.notifyOnChanges = true;

        auto id = repo.insertSchedule(config);
        REQUIRE(id > 0);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Test Scan");
        REQUIRE(retrieved->targetAddress == "192.168.1.1");
        REQUIRE(retrieved->portRange == PortRange::Web);
        REQUIRE(retrieved->intervalMinutes == 30);
        REQUIRE(retrieved->enabled == true);
        REQUIRE(retrieved->notifyOnChanges == true);
    }

    SECTION("Update schedule") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "192.168.1.1";
        config.intervalMinutes = 60;

        auto id = repo.insertSchedule(config);

        config.id = id;
        config.name = "Updated Scan";
        config.intervalMinutes = 120;
        repo.updateSchedule(config);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Updated Scan");
        REQUIRE(retrieved->intervalMinutes == 120);
    }

    SECTION("Remove schedule") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "192.168.1.1";
        config.intervalMinutes = 60;

        auto id = repo.insertSchedule(config);
        repo.removeSchedule(id);

        auto retrieved = repo.findById(id);
        REQUIRE_FALSE(retrieved.has_value());
    }

    SECTION("Find all schedules") {
        ScheduledScanConfig config1;
        config1.name = "Scan 1";
        config1.targetAddress = "192.168.1.1";
        config1.intervalMinutes = 60;

        ScheduledScanConfig config2;
        config2.name = "Scan 2";
        config2.targetAddress = "192.168.1.2";
        config2.intervalMinutes = 30;

        repo.insertSchedule(config1);
        repo.insertSchedule(config2);

        auto all = repo.findAll();
        REQUIRE(all.size() == 2);
    }

    SECTION("Find enabled schedules") {
        ScheduledScanConfig config1;
        config1.name = "Enabled Scan";
        config1.targetAddress = "192.168.1.1";
        config1.intervalMinutes = 60;
        config1.enabled = true;

        ScheduledScanConfig config2;
        config2.name = "Disabled Scan";
        config2.targetAddress = "192.168.1.2";
        config2.intervalMinutes = 30;
        config2.enabled = false;

        repo.insertSchedule(config1);
        repo.insertSchedule(config2);

        auto enabled = repo.findEnabled();
        REQUIRE(enabled.size() == 1);
        REQUIRE(enabled[0].name == "Enabled Scan");
    }

    SECTION("Custom ports serialization") {
        ScheduledScanConfig config;
        config.name = "Custom Scan";
        config.targetAddress = "192.168.1.1";
        config.portRange = PortRange::Custom;
        config.customPorts = {22, 80, 443, 8080, 8443};
        config.intervalMinutes = 60;

        auto id = repo.insertSchedule(config);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->portRange == PortRange::Custom);
        REQUIRE(retrieved->customPorts == std::vector<uint16_t>{22, 80, 443, 8080, 8443});
    }

    SECTION("Insert and retrieve diff") {
        ScheduledScanConfig config;
        config.name = "Test Scan";
        config.targetAddress = "192.168.1.1";
        config.intervalMinutes = 60;
        auto scheduleId = repo.insertSchedule(config);

        PortScanDiff diff;
        diff.targetAddress = "192.168.1.1";
        diff.previousScanTime = std::chrono::system_clock::now() - std::chrono::hours(1);
        diff.currentScanTime = std::chrono::system_clock::now();
        diff.totalPortsScanned = 100;
        diff.openPortsBefore = 3;
        diff.openPortsAfter = 5;

        PortChange change1;
        change1.port = 8080;
        change1.changeType = PortChangeType::NewOpen;
        change1.previousState = PortState::Closed;
        change1.currentState = PortState::Open;
        change1.serviceName = "http-alt";

        PortChange change2;
        change2.port = 8443;
        change2.changeType = PortChangeType::NewOpen;
        change2.previousState = PortState::Closed;
        change2.currentState = PortState::Open;
        change2.serviceName = "https-alt";

        diff.changes = {change1, change2};

        auto diffId = repo.insertDiff(diff, scheduleId);
        REQUIRE(diffId > 0);

        auto diffs = repo.getDiffs(scheduleId);
        REQUIRE(diffs.size() == 1);
        REQUIRE(diffs[0].targetAddress == "192.168.1.1");
        REQUIRE(diffs[0].totalPortsScanned == 100);
        REQUIRE(diffs[0].openPortsBefore == 3);
        REQUIRE(diffs[0].openPortsAfter == 5);
        REQUIRE(diffs[0].changes.size() == 2);
    }

    std::filesystem::remove(testDbPath);
}

TEST_CASE("ScheduledPortScanner callbacks", "[ScheduledPortScan]") {
    AsioContext context;
    context.start();
    PortScanner portScanner(context);
    ScheduledPortScanner scheduler(context, portScanner);

    SECTION("Set scan complete callback") {
        bool callbackInvoked = false;
        scheduler.setScanCompleteCallback(
            [&callbackInvoked](int64_t /*scheduleId*/, const std::vector<PortScanResult>& /*results*/) {
                callbackInvoked = true;
            });

        REQUIRE_FALSE(callbackInvoked);
    }

    SECTION("Set diff callback") {
        bool callbackInvoked = false;
        scheduler.setDiffCallback(
            [&callbackInvoked](int64_t /*scheduleId*/, const PortScanDiff& /*diff*/) {
                callbackInvoked = true;
            });

        REQUIRE_FALSE(callbackInvoked);
    }

    context.stop();
}

TEST_CASE("ScheduledPortScanner last results management", "[ScheduledPortScan]") {
    AsioContext context;
    context.start();
    PortScanner portScanner(context);
    ScheduledPortScanner scheduler(context, portScanner);

    ScheduledScanConfig config;
    config.name = "Test Scan";
    config.targetAddress = "127.0.0.1";
    config.intervalMinutes = 5;

    scheduler.addSchedule(config);
    auto schedules = scheduler.getSchedules();
    auto id = schedules[0].id;

    SECTION("Set and get last results") {
        std::vector<PortScanResult> results = {
            {.targetAddress = "127.0.0.1", .port = 22, .state = PortState::Open},
            {.targetAddress = "127.0.0.1", .port = 80, .state = PortState::Closed},
        };

        scheduler.setLastScanResults(id, results);
        auto retrieved = scheduler.getLastScanResults(id);
        REQUIRE(retrieved.size() == 2);
        REQUIRE(retrieved[0].port == 22);
        REQUIRE(retrieved[1].port == 80);
    }

    SECTION("Get results for non-existent schedule") {
        auto results = scheduler.getLastScanResults(99999);
        REQUIRE(results.empty());
    }

    context.stop();
}
