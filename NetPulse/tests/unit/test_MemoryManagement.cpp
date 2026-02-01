#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>
#include <string>

#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "core/types/Alert.hpp"

using namespace netpulse::core;

// These tests verify proper memory management patterns.
// When run with AddressSanitizer enabled (cmake -DENABLE_ASAN=ON),
// any memory leaks, use-after-free, or buffer overflows will be detected.

TEST_CASE("Smart pointer memory management", "[memory][asan]") {
    SECTION("unique_ptr properly releases memory") {
        auto host = std::make_unique<Host>();
        host->name = "Test Host";
        host->address = "192.168.1.1";
        host->pingIntervalSeconds = 30;
        host->warningThresholdMs = 100;
        host->criticalThresholdMs = 500;

        REQUIRE(host->isValid());
        // unique_ptr automatically deallocates when leaving scope
    }

    SECTION("shared_ptr reference counting") {
        std::shared_ptr<Host> host1 = std::make_shared<Host>();
        host1->name = "Shared Host";
        host1->address = "10.0.0.1";

        {
            std::shared_ptr<Host> host2 = host1;
            REQUIRE(host1.use_count() == 2);
        }

        REQUIRE(host1.use_count() == 1);
        // Memory freed when last shared_ptr goes out of scope
    }

    SECTION("weak_ptr does not prevent deallocation") {
        std::weak_ptr<Host> weakHost;

        {
            auto sharedHost = std::make_shared<Host>();
            sharedHost->name = "Weak Test";
            weakHost = sharedHost;

            REQUIRE_FALSE(weakHost.expired());
        }

        REQUIRE(weakHost.expired());
    }
}

TEST_CASE("Container memory management", "[memory][asan]") {
    SECTION("Vector of objects properly cleaned up") {
        std::vector<Host> hosts;
        hosts.reserve(100);

        for (int i = 0; i < 100; ++i) {
            Host host;
            host.id = i;
            host.name = "Host " + std::to_string(i);
            host.address = "192.168.1." + std::to_string(i % 256);
            host.pingIntervalSeconds = 30;
            host.warningThresholdMs = 100;
            host.criticalThresholdMs = 500;
            hosts.push_back(host);
        }

        REQUIRE(hosts.size() == 100);
        hosts.clear();
        REQUIRE(hosts.empty());
        // Vector destructor handles deallocation
    }

    SECTION("Vector of smart pointers") {
        std::vector<std::unique_ptr<PingResult>> results;

        for (int i = 0; i < 50; ++i) {
            auto result = std::make_unique<PingResult>();
            result->hostId = i;
            result->latency = std::chrono::microseconds(i * 10000);
            result->success = true;
            results.push_back(std::move(result));
        }

        REQUIRE(results.size() == 50);
        // All unique_ptrs freed when vector is destroyed
    }

    SECTION("String memory management") {
        std::vector<std::string> strings;

        for (int i = 0; i < 1000; ++i) {
            strings.emplace_back("Long string content that forces heap allocation " +
                                 std::to_string(i));
        }

        REQUIRE(strings.size() == 1000);
        // String contents are properly freed
    }
}

TEST_CASE("String member memory management", "[memory][asan]") {
    SECTION("Host with string fields") {
        std::vector<Host> hosts;

        for (int i = 0; i < 100; ++i) {
            Host host;
            host.id = i;
            host.name = "Host with a long name that requires heap allocation " + std::to_string(i);
            host.address = "192.168." + std::to_string(i / 256) + "." + std::to_string(i % 256);
            host.pingIntervalSeconds = 30;
            host.warningThresholdMs = 100;
            host.criticalThresholdMs = 500;
            hosts.push_back(host);
        }

        REQUIRE(hosts.size() == 100);

        // Copy semantics
        std::vector<Host> hostsCopy = hosts;
        REQUIRE(hostsCopy.size() == 100);

        // Move semantics
        std::vector<Host> hostsMoved = std::move(hostsCopy);
        REQUIRE(hostsMoved.size() == 100);
    }
}

TEST_CASE("Alert memory management", "[memory][asan]") {
    SECTION("Alert creation and destruction") {
        std::vector<std::unique_ptr<Alert>> alerts;

        for (int i = 0; i < 50; ++i) {
            auto alert = std::make_unique<Alert>();
            alert->hostId = i;
            alert->severity = static_cast<AlertSeverity>(i % 4);
            alert->message = "Alert message " + std::to_string(i);
            alerts.push_back(std::move(alert));
        }

        REQUIRE(alerts.size() == 50);
        alerts.erase(alerts.begin(), alerts.begin() + 25);
        REQUIRE(alerts.size() == 25);
    }
}

TEST_CASE("Exception safety and memory", "[memory][asan]") {
    SECTION("RAII with exceptions") {
        bool caughtException = false;

        try {
            auto host = std::make_unique<Host>();
            host->name = "Exception Test";

            std::vector<int> vec;
            // This would throw if we tried vec.at(1000) but we'll simulate
            // proper RAII cleanup happens even with exceptions

            throw std::runtime_error("Test exception");
        } catch (const std::runtime_error&) {
            caughtException = true;
            // host unique_ptr is properly destroyed during stack unwinding
        }

        REQUIRE(caughtException);
    }
}

TEST_CASE("Move semantics prevent copies", "[memory][asan]") {
    SECTION("Move constructor avoids allocation") {
        Host original;
        original.name = "Original Host";
        original.address = "192.168.1.1";
        original.pingIntervalSeconds = 30;
        original.warningThresholdMs = 100;
        original.criticalThresholdMs = 500;

        Host moved = std::move(original);
        REQUIRE(moved.name == "Original Host");

        // original is in valid but unspecified state after move
    }

    SECTION("Vector move semantics") {
        std::vector<Host> source;
        source.reserve(100);

        for (int i = 0; i < 100; ++i) {
            Host host;
            host.id = i;
            host.name = "Host " + std::to_string(i);
            source.push_back(std::move(host));
        }

        std::vector<Host> destination = std::move(source);
        REQUIRE(destination.size() == 100);
        REQUIRE(source.empty());  // NOLINT: testing moved-from state
    }
}

TEST_CASE("PingResult bulk operations", "[memory][asan]") {
    SECTION("Large batch of ping results") {
        std::vector<PingResult> results;
        results.reserve(10000);

        for (int i = 0; i < 10000; ++i) {
            PingResult result;
            result.hostId = i % 100;
            result.latency = std::chrono::microseconds(i * 1000);
            result.success = (i % 10) != 0;
            results.push_back(result);
        }

        REQUIRE(results.size() == 10000);

        // Simulating data retention cleanup
        auto it = std::remove_if(results.begin(), results.end(),
                                 [](const PingResult& r) { return !r.success; });
        results.erase(it, results.end());

        REQUIRE(results.size() == 9000);
    }
}
