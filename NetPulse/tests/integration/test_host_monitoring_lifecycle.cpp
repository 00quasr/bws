#include <catch2/catch_test_macros.hpp>

#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "infrastructure/network/AsioContext.hpp"
#include "infrastructure/network/PingService.hpp"
#include "viewmodels/DashboardViewModel.hpp"

#include <QCoreApplication>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

using namespace netpulse::core;
using namespace netpulse::infra;
using namespace netpulse::viewmodels;

namespace {

class IntegrationTestDatabase {
public:
    IntegrationTestDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_monitoring_integration_test.db") {
        std::filesystem::remove(dbPath_);
        db_ = std::make_shared<Database>(dbPath_.string());
        db_->runMigrations();
    }

    ~IntegrationTestDatabase() {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::shared_ptr<Database> get() { return db_; }

private:
    std::filesystem::path dbPath_;
    std::shared_ptr<Database> db_;
};

Host createTestHost(const std::string& name, const std::string& address, int intervalSeconds = 1) {
    Host host;
    host.name = name;
    host.address = address;
    host.pingIntervalSeconds = intervalSeconds;
    host.warningThresholdMs = 100;
    host.criticalThresholdMs = 500;
    host.status = HostStatus::Unknown;
    host.enabled = true;
    host.createdAt = std::chrono::system_clock::now();
    return host;
}

} // namespace

// =============================================================================
// Host Monitoring Lifecycle Integration Tests
// =============================================================================

TEST_CASE("Host monitoring lifecycle - start and stop monitoring",
          "[Integration][Monitoring][Lifecycle]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);

    SECTION("PingService tracks monitoring state correctly") {
        Host host = createTestHost("Monitor Host", "127.0.0.1");
        int64_t hostId = hostRepo.insert(host);
        host.id = hostId;

        REQUIRE_FALSE(pingService->isMonitoring(hostId));

        pingService->startMonitoring(host, [](const PingResult&) {});

        REQUIRE(pingService->isMonitoring(hostId));

        pingService->stopMonitoring(hostId);

        REQUIRE_FALSE(pingService->isMonitoring(hostId));
    }

    SECTION("Multiple hosts can be monitored simultaneously") {
        Host host1 = createTestHost("Host 1", "192.168.50.1");
        Host host2 = createTestHost("Host 2", "192.168.50.2");
        Host host3 = createTestHost("Host 3", "192.168.50.3");

        int64_t id1 = hostRepo.insert(host1);
        int64_t id2 = hostRepo.insert(host2);
        int64_t id3 = hostRepo.insert(host3);

        host1.id = id1;
        host2.id = id2;
        host3.id = id3;

        pingService->startMonitoring(host1, [](const PingResult&) {});
        pingService->startMonitoring(host2, [](const PingResult&) {});
        pingService->startMonitoring(host3, [](const PingResult&) {});

        REQUIRE(pingService->isMonitoring(id1));
        REQUIRE(pingService->isMonitoring(id2));
        REQUIRE(pingService->isMonitoring(id3));

        pingService->stopMonitoring(id2);

        REQUIRE(pingService->isMonitoring(id1));
        REQUIRE_FALSE(pingService->isMonitoring(id2));
        REQUIRE(pingService->isMonitoring(id3));

        pingService->stopAllMonitoring();

        REQUIRE_FALSE(pingService->isMonitoring(id1));
        REQUIRE_FALSE(pingService->isMonitoring(id2));
        REQUIRE_FALSE(pingService->isMonitoring(id3));
    }

    context.stop();
}

TEST_CASE("Host monitoring lifecycle - ping callbacks are invoked",
          "[Integration][Monitoring][Callback]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);

    SECTION("Callback receives ping results") {
        Host host = createTestHost("Callback Host", "127.0.0.1", 1);
        int64_t hostId = hostRepo.insert(host);
        host.id = hostId;

        std::mutex mtx;
        std::condition_variable cv;
        std::vector<PingResult> results;
        bool hasResult = false;

        pingService->startMonitoring(host, [&](const PingResult& result) {
            std::lock_guard<std::mutex> lock(mtx);
            results.push_back(result);
            hasResult = true;
            cv.notify_one();
        });

        // Wait for at least one result with timeout
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(10), [&] { return hasResult; });
        }

        pingService->stopMonitoring(hostId);

        REQUIRE_FALSE(results.empty());
        REQUIRE(results[0].timestamp.time_since_epoch().count() > 0);
    }

    context.stop();
}

TEST_CASE("Host monitoring lifecycle - results stored in database",
          "[Integration][Monitoring][Database]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    SECTION("Ping results are persisted to database via manual storage") {
        Host host = createTestHost("DB Host", "127.0.0.1", 1);
        int64_t hostId = hostRepo.insert(host);
        host.id = hostId;

        std::mutex mtx;
        std::condition_variable cv;
        int resultCount = 0;

        pingService->startMonitoring(host, [&](const PingResult& result) {
            std::lock_guard<std::mutex> lock(mtx);
            PingResult toStore = result;
            toStore.hostId = hostId;
            metricsRepo.insertPingResult(toStore);
            resultCount++;
            cv.notify_one();
        });

        // Wait for at least 2 results
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(15), [&] { return resultCount >= 2; });
        }

        pingService->stopMonitoring(hostId);

        auto storedResults = metricsRepo.getPingResults(hostId, 100);
        REQUIRE(storedResults.size() >= 2);

        for (const auto& result : storedResults) {
            REQUIRE(result.hostId == hostId);
            REQUIRE(result.timestamp.time_since_epoch().count() > 0);
        }
    }

    context.stop();
}

TEST_CASE("Host monitoring lifecycle - statistics updated after monitoring",
          "[Integration][Monitoring][Statistics]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);
    MetricsRepository metricsRepo(db);

    SECTION("Statistics reflect stored ping results") {
        Host host = createTestHost("Stats Host", "127.0.0.1", 1);
        int64_t hostId = hostRepo.insert(host);
        host.id = hostId;

        std::mutex mtx;
        std::condition_variable cv;
        int resultCount = 0;

        pingService->startMonitoring(host, [&](const PingResult& result) {
            std::lock_guard<std::mutex> lock(mtx);
            PingResult toStore = result;
            toStore.hostId = hostId;
            metricsRepo.insertPingResult(toStore);
            resultCount++;
            cv.notify_one();
        });

        // Wait for at least 3 results
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(20), [&] { return resultCount >= 3; });
        }

        pingService->stopMonitoring(hostId);

        auto stats = metricsRepo.getStatistics(hostId);
        REQUIRE(stats.hostId == hostId);
        REQUIRE(stats.totalPings >= 3);
    }

    context.stop();
}

TEST_CASE("Host monitoring lifecycle - DashboardViewModel integration",
          "[Integration][Monitoring][ViewModel]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);

    SECTION("DashboardViewModel can be constructed with dependencies") {
        REQUIRE_NOTHROW(DashboardViewModel(db, pingService));
    }

    SECTION("DashboardViewModel retrieves hosts from database") {
        Host host1 = createTestHost("Dashboard Host 1", "192.168.60.1");
        Host host2 = createTestHost("Dashboard Host 2", "192.168.60.2");

        hostRepo.insert(host1);
        hostRepo.insert(host2);

        DashboardViewModel dashboard(db, pingService);

        auto hosts = dashboard.getHosts();
        REQUIRE(hosts.size() == 2);
    }

    SECTION("DashboardViewModel tracks host counts") {
        Host host1 = createTestHost("Count Host 1", "192.168.70.1");
        Host host2 = createTestHost("Count Host 2", "192.168.70.2");

        hostRepo.insert(host1);
        hostRepo.insert(host2);

        DashboardViewModel dashboard(db, pingService);

        REQUIRE(dashboard.hostCount() == 2);
    }

    context.stop();
}

TEST_CASE("Host monitoring lifecycle - restart monitoring after stop",
          "[Integration][Monitoring][Restart]") {
    IntegrationTestDatabase testDb;
    auto db = testDb.get();

    AsioContext context;
    context.start();
    auto pingService = std::make_shared<PingService>(context);

    HostRepository hostRepo(db);

    SECTION("Monitoring can be restarted after stopping") {
        Host host = createTestHost("Restart Host", "127.0.0.1", 1);
        int64_t hostId = hostRepo.insert(host);
        host.id = hostId;

        std::atomic<int> callbackCount{0};

        // First monitoring session
        pingService->startMonitoring(host, [&](const PingResult&) {
            callbackCount++;
        });

        REQUIRE(pingService->isMonitoring(hostId));

        // Stop monitoring
        pingService->stopMonitoring(hostId);
        REQUIRE_FALSE(pingService->isMonitoring(hostId));

        int countAfterStop = callbackCount.load();

        // Wait a moment to ensure no more callbacks
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        REQUIRE(callbackCount.load() == countAfterStop);

        // Restart monitoring
        pingService->startMonitoring(host, [&](const PingResult&) {
            callbackCount++;
        });

        REQUIRE(pingService->isMonitoring(hostId));

        // Wait for new results
        std::this_thread::sleep_for(std::chrono::seconds(3));

        pingService->stopMonitoring(hostId);

        REQUIRE(callbackCount.load() > countAfterStop);
    }

    context.stop();
}
