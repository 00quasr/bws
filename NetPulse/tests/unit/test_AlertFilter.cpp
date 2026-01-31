#include <catch2/catch_test_macros.hpp>

#include "core/types/Alert.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/MetricsRepository.hpp"

#include <filesystem>

using namespace netpulse::infra;
using namespace netpulse::core;

namespace {

class TestDatabase {
public:
    TestDatabase() : dbPath_(std::filesystem::temp_directory_path() / "netpulse_filter_test.db") {
        std::filesystem::remove(dbPath_);
        db_ = std::make_shared<Database>(dbPath_.string());
        db_->runMigrations();
    }

    ~TestDatabase() {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::shared_ptr<Database> get() { return db_; }

private:
    std::filesystem::path dbPath_;
    std::shared_ptr<Database> db_;
};

void insertTestAlert(MetricsRepository& repo, int64_t hostId, AlertType type, AlertSeverity severity,
                     const std::string& title, const std::string& message, bool acknowledged) {
    Alert alert;
    alert.hostId = hostId;
    alert.type = type;
    alert.severity = severity;
    alert.title = title;
    alert.message = message;
    alert.timestamp = std::chrono::system_clock::now();
    alert.acknowledged = acknowledged;
    repo.insertAlert(alert);
}

} // namespace

TEST_CASE("AlertFilter struct", "[Alert][Filter]") {
    SECTION("isEmpty returns true for empty filter") {
        AlertFilter filter;
        CHECK(filter.isEmpty());
    }

    SECTION("isEmpty returns false when severity is set") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Critical;
        CHECK_FALSE(filter.isEmpty());
    }

    SECTION("isEmpty returns false when type is set") {
        AlertFilter filter;
        filter.type = AlertType::HostDown;
        CHECK_FALSE(filter.isEmpty());
    }

    SECTION("isEmpty returns false when acknowledged is set") {
        AlertFilter filter;
        filter.acknowledged = false;
        CHECK_FALSE(filter.isEmpty());
    }

    SECTION("isEmpty returns false when searchText is set") {
        AlertFilter filter;
        filter.searchText = "test";
        CHECK_FALSE(filter.isEmpty());
    }
}

TEST_CASE("MetricsRepository getAlertsFiltered", "[Database][Alert][Filter]") {
    TestDatabase testDb;
    MetricsRepository repo(testDb.get());

    int64_t hostId = 1;

    insertTestAlert(repo, hostId, AlertType::HostDown, AlertSeverity::Critical, "Host Down",
                    "Server 1 is not responding", false);
    insertTestAlert(repo, hostId, AlertType::HighLatency, AlertSeverity::Warning, "High Latency",
                    "Server 2 has high latency", false);
    insertTestAlert(repo, hostId, AlertType::HostRecovered, AlertSeverity::Info, "Host Recovered",
                    "Server 1 is back online", true);
    insertTestAlert(repo, hostId, AlertType::PacketLoss, AlertSeverity::Critical, "Packet Loss",
                    "Network experiencing packet loss", false);

    SECTION("Empty filter returns all alerts") {
        AlertFilter filter;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 4);
    }

    SECTION("Filter by severity Critical") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Critical;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 2);
        for (const auto& alert : alerts) {
            CHECK(alert.severity == AlertSeverity::Critical);
        }
    }

    SECTION("Filter by severity Warning") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Warning;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].title == "High Latency");
    }

    SECTION("Filter by severity Info") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Info;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].title == "Host Recovered");
    }

    SECTION("Filter by type HostDown") {
        AlertFilter filter;
        filter.type = AlertType::HostDown;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].title == "Host Down");
    }

    SECTION("Filter by type HighLatency") {
        AlertFilter filter;
        filter.type = AlertType::HighLatency;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].title == "High Latency");
    }

    SECTION("Filter by acknowledged status - active") {
        AlertFilter filter;
        filter.acknowledged = false;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 3);
        for (const auto& alert : alerts) {
            CHECK_FALSE(alert.acknowledged);
        }
    }

    SECTION("Filter by acknowledged status - acknowledged") {
        AlertFilter filter;
        filter.acknowledged = true;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].acknowledged);
        CHECK(alerts[0].title == "Host Recovered");
    }

    SECTION("Search by title") {
        AlertFilter filter;
        filter.searchText = "Host";
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 2);
    }

    SECTION("Search by message") {
        AlertFilter filter;
        filter.searchText = "Server 1";
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 2);
    }

    SECTION("Search is case insensitive in SQLite") {
        AlertFilter filter;
        filter.searchText = "HOST";
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 2);
    }

    SECTION("Combined filters - severity and type") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Critical;
        filter.type = AlertType::HostDown;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].title == "Host Down");
    }

    SECTION("Combined filters - severity and acknowledged") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Critical;
        filter.acknowledged = false;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 2);
    }

    SECTION("Combined filters - type and search") {
        AlertFilter filter;
        filter.type = AlertType::HighLatency;
        filter.searchText = "latency";
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].title == "High Latency");
    }

    SECTION("All filters combined") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Critical;
        filter.type = AlertType::HostDown;
        filter.acknowledged = false;
        filter.searchText = "Server";
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.size() == 1);
        CHECK(alerts[0].title == "Host Down");
    }

    SECTION("Filter with no matching results") {
        AlertFilter filter;
        filter.severity = AlertSeverity::Warning;
        filter.type = AlertType::HostDown;
        auto alerts = repo.getAlertsFiltered(filter, 100);
        CHECK(alerts.empty());
    }

    SECTION("Limit parameter works") {
        AlertFilter filter;
        auto alerts = repo.getAlertsFiltered(filter, 2);
        CHECK(alerts.size() == 2);
    }
}
