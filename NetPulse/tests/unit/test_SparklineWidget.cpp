#include <catch2/catch_test_macros.hpp>

#include "core/types/Host.hpp"

#include <deque>

// Test the SparklineWidget data management logic without Qt dependencies
// Since SparklineWidget is a QWidget, we test the data structures and logic separately

using namespace netpulse::core;

namespace {

struct TestDataPoint {
    double latencyMs{0.0};
    bool success{true};
};

class SparklineDataManager {
public:
    void setMaxDataPoints(int maxPoints) {
        maxDataPoints_ = std::max(2, maxPoints);
        while (static_cast<int>(data_.size()) > maxDataPoints_) {
            data_.pop_front();
        }
    }

    int maxDataPoints() const { return maxDataPoints_; }
    size_t dataSize() const { return data_.size(); }

    void addDataPoint(double latencyMs, bool success) {
        data_.push_back({latencyMs, success});
        while (static_cast<int>(data_.size()) > maxDataPoints_) {
            data_.pop_front();
        }
    }

    void setData(const std::deque<double>& latencies, const std::deque<bool>& successes) {
        data_.clear();
        size_t count = std::min(latencies.size(), successes.size());
        for (size_t i = 0; i < count; ++i) {
            data_.push_back({latencies[i], successes[i]});
        }
        while (static_cast<int>(data_.size()) > maxDataPoints_) {
            data_.pop_front();
        }
    }

    void clear() { data_.clear(); }

    const std::deque<TestDataPoint>& data() const { return data_; }

private:
    std::deque<TestDataPoint> data_;
    int maxDataPoints_{30};
};

} // namespace

TEST_CASE("SparklineDataManager initialization", "[SparklineWidget]") {
    SparklineDataManager manager;

    REQUIRE(manager.maxDataPoints() == 30);
    REQUIRE(manager.dataSize() == 0);
}

TEST_CASE("SparklineDataManager addDataPoint", "[SparklineWidget]") {
    SparklineDataManager manager;

    SECTION("Adding single data point") {
        manager.addDataPoint(50.0, true);
        REQUIRE(manager.dataSize() == 1);
        REQUIRE(manager.data()[0].latencyMs == 50.0);
        REQUIRE(manager.data()[0].success == true);
    }

    SECTION("Adding multiple data points") {
        manager.addDataPoint(10.0, true);
        manager.addDataPoint(20.0, true);
        manager.addDataPoint(30.0, false);

        REQUIRE(manager.dataSize() == 3);
        REQUIRE(manager.data()[0].latencyMs == 10.0);
        REQUIRE(manager.data()[1].latencyMs == 20.0);
        REQUIRE(manager.data()[2].latencyMs == 30.0);
        REQUIRE(manager.data()[2].success == false);
    }

    SECTION("Respects maxDataPoints limit") {
        manager.setMaxDataPoints(5);

        for (int i = 0; i < 10; ++i) {
            manager.addDataPoint(static_cast<double>(i * 10), true);
        }

        REQUIRE(manager.dataSize() == 5);
        // Oldest points should be removed
        REQUIRE(manager.data()[0].latencyMs == 50.0);
        REQUIRE(manager.data()[4].latencyMs == 90.0);
    }
}

TEST_CASE("SparklineDataManager setMaxDataPoints", "[SparklineWidget]") {
    SparklineDataManager manager;

    SECTION("Setting max points") {
        manager.setMaxDataPoints(20);
        REQUIRE(manager.maxDataPoints() == 20);
    }

    SECTION("Minimum max points is 2") {
        manager.setMaxDataPoints(1);
        REQUIRE(manager.maxDataPoints() == 2);

        manager.setMaxDataPoints(0);
        REQUIRE(manager.maxDataPoints() == 2);

        manager.setMaxDataPoints(-5);
        REQUIRE(manager.maxDataPoints() == 2);
    }

    SECTION("Reducing max points trims existing data") {
        for (int i = 0; i < 10; ++i) {
            manager.addDataPoint(static_cast<double>(i), true);
        }
        REQUIRE(manager.dataSize() == 10);

        manager.setMaxDataPoints(5);
        REQUIRE(manager.dataSize() == 5);
        // Should keep the newest data points
        REQUIRE(manager.data()[0].latencyMs == 5.0);
    }
}

TEST_CASE("SparklineDataManager setData", "[SparklineWidget]") {
    SparklineDataManager manager;

    SECTION("Setting data from deques") {
        std::deque<double> latencies = {10.0, 20.0, 30.0, 40.0};
        std::deque<bool> successes = {true, true, false, true};

        manager.setData(latencies, successes);

        REQUIRE(manager.dataSize() == 4);
        REQUIRE(manager.data()[0].latencyMs == 10.0);
        REQUIRE(manager.data()[2].success == false);
    }

    SECTION("Handles mismatched deque sizes") {
        std::deque<double> latencies = {10.0, 20.0, 30.0, 40.0, 50.0};
        std::deque<bool> successes = {true, true, false};

        manager.setData(latencies, successes);

        // Should only use minimum of the two sizes
        REQUIRE(manager.dataSize() == 3);
    }

    SECTION("Respects maxDataPoints when setting data") {
        manager.setMaxDataPoints(3);

        std::deque<double> latencies = {10.0, 20.0, 30.0, 40.0, 50.0};
        std::deque<bool> successes = {true, true, true, true, true};

        manager.setData(latencies, successes);

        REQUIRE(manager.dataSize() == 3);
        // Oldest points should be trimmed
        REQUIRE(manager.data()[0].latencyMs == 30.0);
    }

    SECTION("Setting data clears previous data") {
        manager.addDataPoint(100.0, true);
        manager.addDataPoint(200.0, true);

        std::deque<double> latencies = {1.0, 2.0};
        std::deque<bool> successes = {true, true};

        manager.setData(latencies, successes);

        REQUIRE(manager.dataSize() == 2);
        REQUIRE(manager.data()[0].latencyMs == 1.0);
    }
}

TEST_CASE("SparklineDataManager clear", "[SparklineWidget]") {
    SparklineDataManager manager;

    manager.addDataPoint(10.0, true);
    manager.addDataPoint(20.0, true);
    REQUIRE(manager.dataSize() == 2);

    manager.clear();
    REQUIRE(manager.dataSize() == 0);
}

TEST_CASE("HostStatus color mapping", "[SparklineWidget]") {
    // Test the host status enum values that are used for color selection
    SECTION("HostStatus values") {
        REQUIRE(static_cast<int>(HostStatus::Unknown) == 0);
        REQUIRE(static_cast<int>(HostStatus::Up) == 1);
        REQUIRE(static_cast<int>(HostStatus::Warning) == 2);
        REQUIRE(static_cast<int>(HostStatus::Down) == 3);
    }
}
