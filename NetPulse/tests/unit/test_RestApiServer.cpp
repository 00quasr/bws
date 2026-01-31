#include <catch2/catch_test_macros.hpp>

#include "infrastructure/api/RestApiServer.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

using namespace netpulse;

namespace {

// Helper to create a test database in memory
std::shared_ptr<infra::Database> createTestDatabase() {
    auto db = std::make_shared<infra::Database>(":memory:");
    db->runMigrations();
    return db;
}

// Simple HTTP client for testing
class TestHttpClient {
public:
    explicit TestHttpClient(asio::io_context& io) : io_(io), socket_(io) {}

    std::pair<int, std::string> request(const std::string& method, const std::string& path,
                                        const std::string& body = "",
                                        const std::map<std::string, std::string>& headers = {}) {
        try {
            asio::ip::tcp::resolver resolver(io_);
            auto endpoints = resolver.resolve("127.0.0.1", std::to_string(testPort_));
            asio::connect(socket_, endpoints);

            // Build request
            std::ostringstream request;
            request << method << " " << path << " HTTP/1.1\r\n";
            request << "Host: localhost\r\n";
            for (const auto& [key, value] : headers) {
                request << key << ": " << value << "\r\n";
            }
            if (!body.empty()) {
                request << "Content-Length: " << body.size() << "\r\n";
                request << "Content-Type: application/json\r\n";
            }
            request << "Connection: close\r\n\r\n";
            request << body;

            asio::write(socket_, asio::buffer(request.str()));

            // Read response
            asio::streambuf responseBuffer;
            asio::error_code ec;
            asio::read_until(socket_, responseBuffer, "\r\n\r\n", ec);

            std::string response((std::istreambuf_iterator<char>(&responseBuffer)),
                                 std::istreambuf_iterator<char>());

            // Read remaining body
            std::vector<char> bodyBuf(8192);
            size_t len = socket_.read_some(asio::buffer(bodyBuf), ec);
            if (len > 0) {
                response += std::string(bodyBuf.data(), len);
            }

            socket_.close();

            // Parse status code
            int statusCode = 0;
            auto statusLine = response.substr(0, response.find("\r\n"));
            auto space1 = statusLine.find(' ');
            auto space2 = statusLine.find(' ', space1 + 1);
            if (space1 != std::string::npos && space2 != std::string::npos) {
                statusCode = std::stoi(statusLine.substr(space1 + 1, space2 - space1 - 1));
            }

            // Extract body
            auto bodyStart = response.find("\r\n\r\n");
            std::string responseBody;
            if (bodyStart != std::string::npos) {
                responseBody = response.substr(bodyStart + 4);
            }

            return {statusCode, responseBody};
        } catch (const std::exception& e) {
            return {0, std::string("Error: ") + e.what()};
        }
    }

    void setPort(uint16_t port) { testPort_ = port; }

private:
    asio::io_context& io_;
    asio::ip::tcp::socket socket_;
    uint16_t testPort_{8181};
};

} // namespace

TEST_CASE("ApiResponse formatting", "[RestApi]") {
    SECTION("setJson sets correct headers and body") {
        infra::ApiResponse response;
        nlohmann::json j;
        j["key"] = "value";
        j["number"] = 42;

        response.setJson(j);

        REQUIRE(response.headers["Content-Type"] == "application/json");
        REQUIRE(response.body == j.dump());
    }

    SECTION("setError sets correct status and body") {
        infra::ApiResponse response;
        response.setError(404, "Resource not found");

        REQUIRE(response.statusCode == 404);
        REQUIRE(response.statusText == "Not Found");

        auto json = nlohmann::json::parse(response.body);
        REQUIRE(json["error"] == "Resource not found");
        REQUIRE(json["status"] == 404);
    }

    SECTION("toString produces valid HTTP response") {
        infra::ApiResponse response;
        response.statusCode = 200;
        response.statusText = "OK";
        response.body = R"({"message":"hello"})";
        response.headers["Content-Type"] = "application/json";

        std::string httpResponse = response.toString();

        REQUIRE(httpResponse.find("HTTP/1.1 200 OK") != std::string::npos);
        REQUIRE(httpResponse.find("Content-Type: application/json") != std::string::npos);
        REQUIRE(httpResponse.find("Content-Length: 19") != std::string::npos);
        REQUIRE(httpResponse.find(R"({"message":"hello"})") != std::string::npos);
    }
}

TEST_CASE("RestApiServer lifecycle", "[RestApi]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();

    SECTION("Server starts and stops correctly") {
        auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8182);

        REQUIRE_FALSE(server->isRunning());

        server->start();
        REQUIRE(server->isRunning());
        REQUIRE(server->port() == 8182);

        server->stop();
        REQUIRE_FALSE(server->isRunning());
    }

    SECTION("Server can be started multiple times") {
        auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8183);

        server->start();
        server->stop();
        server->start();
        REQUIRE(server->isRunning());
        server->stop();
    }

    asioContext.stop();
}

TEST_CASE("RestApiServer health endpoint", "[RestApi][Integration]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();
    auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8184);
    server->start();

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::io_context clientIo;
    TestHttpClient client(clientIo);
    client.setPort(8184);

    SECTION("Health endpoint returns healthy status") {
        auto [status, body] = client.request("GET", "/api/health");

        REQUIRE(status == 200);
        auto json = nlohmann::json::parse(body);
        REQUIRE(json["status"] == "healthy");
        REQUIRE(json.contains("timestamp"));
        REQUIRE(json.contains("version"));
    }

    server->stop();
    asioContext.stop();
}

TEST_CASE("RestApiServer authentication", "[RestApi][Integration]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();
    auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8185);
    server->setApiKey("test-secret-key");
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::io_context clientIo;
    TestHttpClient client(clientIo);
    client.setPort(8185);

    SECTION("Request without API key is rejected") {
        auto [status, body] = client.request("GET", "/api/hosts");

        REQUIRE(status == 401);
        auto json = nlohmann::json::parse(body);
        REQUIRE(json["error"].get<std::string>().find("API key") != std::string::npos);
    }

    SECTION("Request with valid X-API-Key header succeeds") {
        auto [status, body] = client.request("GET", "/api/hosts", "",
                                             {{"X-API-Key", "test-secret-key"}});

        REQUIRE(status == 200);
    }

    SECTION("Request with invalid API key is rejected") {
        auto [status, body] = client.request("GET", "/api/hosts", "",
                                             {{"X-API-Key", "wrong-key"}});

        REQUIRE(status == 401);
    }

    SECTION("Health endpoint does not require authentication") {
        auto [status, body] = client.request("GET", "/api/health");

        REQUIRE(status == 200);
    }

    server->stop();
    asioContext.stop();
}

TEST_CASE("RestApiServer host CRUD", "[RestApi][Integration]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();
    auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8186);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::io_context clientIo;
    TestHttpClient client(clientIo);
    client.setPort(8186);

    SECTION("Create and retrieve host") {
        nlohmann::json createBody;
        createBody["name"] = "Test Server";
        createBody["address"] = "192.168.1.100";
        createBody["pingIntervalSeconds"] = 60;

        auto [createStatus, createResponse] = client.request("POST", "/api/hosts",
                                                             createBody.dump());

        REQUIRE(createStatus == 201);
        auto created = nlohmann::json::parse(createResponse);
        REQUIRE(created["host"]["name"] == "Test Server");
        REQUIRE(created["host"]["address"] == "192.168.1.100");

        int64_t hostId = created["host"]["id"];

        // Retrieve the host
        auto [getStatus, getResponse] = client.request("GET", "/api/hosts/" + std::to_string(hostId));

        REQUIRE(getStatus == 200);
        auto retrieved = nlohmann::json::parse(getResponse);
        REQUIRE(retrieved["name"] == "Test Server");
    }

    SECTION("Get all hosts") {
        // Create a couple of hosts
        nlohmann::json host1;
        host1["name"] = "Server 1";
        host1["address"] = "10.0.0.1";
        client.request("POST", "/api/hosts", host1.dump());

        nlohmann::json host2;
        host2["name"] = "Server 2";
        host2["address"] = "10.0.0.2";
        client.request("POST", "/api/hosts", host2.dump());

        auto [status, body] = client.request("GET", "/api/hosts");

        REQUIRE(status == 200);
        auto response = nlohmann::json::parse(body);
        REQUIRE(response["count"].get<int>() >= 2);
        REQUIRE(response["hosts"].is_array());
    }

    SECTION("Update host") {
        nlohmann::json createBody;
        createBody["name"] = "Original Name";
        createBody["address"] = "10.0.0.50";

        auto [createStatus, createResponse] = client.request("POST", "/api/hosts",
                                                             createBody.dump());
        auto created = nlohmann::json::parse(createResponse);
        int64_t hostId = created["host"]["id"];

        nlohmann::json updateBody;
        updateBody["name"] = "Updated Name";
        updateBody["pingIntervalSeconds"] = 120;

        auto [updateStatus, updateResponse] = client.request("PUT",
                                                             "/api/hosts/" + std::to_string(hostId),
                                                             updateBody.dump());

        REQUIRE(updateStatus == 200);
        auto updated = nlohmann::json::parse(updateResponse);
        REQUIRE(updated["host"]["name"] == "Updated Name");
        REQUIRE(updated["host"]["pingIntervalSeconds"] == 120);
    }

    SECTION("Delete host") {
        nlohmann::json createBody;
        createBody["name"] = "To Delete";
        createBody["address"] = "10.0.0.99";

        auto [createStatus, createResponse] = client.request("POST", "/api/hosts",
                                                             createBody.dump());
        auto created = nlohmann::json::parse(createResponse);
        int64_t hostId = created["host"]["id"];

        auto [deleteStatus, deleteResponse] = client.request("DELETE",
                                                             "/api/hosts/" + std::to_string(hostId));

        REQUIRE(deleteStatus == 200);

        // Verify host is deleted
        auto [getStatus, getResponse] = client.request("GET", "/api/hosts/" + std::to_string(hostId));
        REQUIRE(getStatus == 404);
    }

    SECTION("Get non-existent host returns 404") {
        auto [status, body] = client.request("GET", "/api/hosts/99999");

        REQUIRE(status == 404);
    }

    SECTION("Create host with invalid data returns 400") {
        nlohmann::json invalidHost;
        invalidHost["name"] = "";  // Empty name is invalid
        invalidHost["address"] = "192.168.1.1";

        auto [status, body] = client.request("POST", "/api/hosts", invalidHost.dump());

        REQUIRE(status == 400);
    }

    server->stop();
    asioContext.stop();
}

TEST_CASE("RestApiServer alert endpoints", "[RestApi][Integration]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();

    // Insert a test alert
    infra::MetricsRepository metricsRepo(db);
    core::Alert testAlert;
    testAlert.hostId = 1;
    testAlert.type = core::AlertType::HostDown;
    testAlert.severity = core::AlertSeverity::Critical;
    testAlert.title = "Test Alert";
    testAlert.message = "Test host is down";
    testAlert.timestamp = std::chrono::system_clock::now();
    testAlert.acknowledged = false;
    int64_t alertId = metricsRepo.insertAlert(testAlert);

    auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8187);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::io_context clientIo;
    TestHttpClient client(clientIo);
    client.setPort(8187);

    SECTION("Get alerts") {
        auto [status, body] = client.request("GET", "/api/alerts");

        REQUIRE(status == 200);
        auto response = nlohmann::json::parse(body);
        REQUIRE(response["alerts"].is_array());
        REQUIRE(response["count"].get<int>() >= 1);
    }

    SECTION("Get alerts with filter") {
        auto [status, body] = client.request("GET", "/api/alerts?severity=Critical&limit=10");

        REQUIRE(status == 200);
        auto response = nlohmann::json::parse(body);
        for (const auto& alert : response["alerts"]) {
            REQUIRE(alert["severity"] == "Critical");
        }
    }

    SECTION("Acknowledge alert") {
        auto [status, body] = client.request("POST",
                                             "/api/alerts/" + std::to_string(alertId) + "/acknowledge");

        REQUIRE(status == 200);
        auto response = nlohmann::json::parse(body);
        REQUIRE(response["message"].get<std::string>().find("acknowledged") != std::string::npos);
    }

    SECTION("Acknowledge all alerts") {
        auto [status, body] = client.request("POST", "/api/alerts/acknowledge-all");

        REQUIRE(status == 200);
        auto response = nlohmann::json::parse(body);
        REQUIRE(response["message"].get<std::string>().find("acknowledged") != std::string::npos);
    }

    server->stop();
    asioContext.stop();
}

TEST_CASE("RestApiServer group endpoints", "[RestApi][Integration]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();
    auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8188);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::io_context clientIo;
    TestHttpClient client(clientIo);
    client.setPort(8188);

    SECTION("Create and retrieve group") {
        nlohmann::json createBody;
        createBody["name"] = "Production Servers";
        createBody["description"] = "All production servers";

        auto [createStatus, createResponse] = client.request("POST", "/api/groups",
                                                             createBody.dump());

        REQUIRE(createStatus == 201);
        auto created = nlohmann::json::parse(createResponse);
        REQUIRE(created["group"]["name"] == "Production Servers");

        int64_t groupId = created["group"]["id"];

        // Retrieve the group
        auto [getStatus, getResponse] = client.request("GET", "/api/groups/" + std::to_string(groupId));

        REQUIRE(getStatus == 200);
        auto retrieved = nlohmann::json::parse(getResponse);
        REQUIRE(retrieved["name"] == "Production Servers");
    }

    SECTION("Get all groups") {
        nlohmann::json group;
        group["name"] = "Test Group";
        client.request("POST", "/api/groups", group.dump());

        auto [status, body] = client.request("GET", "/api/groups");

        REQUIRE(status == 200);
        auto response = nlohmann::json::parse(body);
        REQUIRE(response["groups"].is_array());
    }

    SECTION("Delete group") {
        nlohmann::json createBody;
        createBody["name"] = "To Delete";

        auto [createStatus, createResponse] = client.request("POST", "/api/groups",
                                                             createBody.dump());
        auto created = nlohmann::json::parse(createResponse);
        int64_t groupId = created["group"]["id"];

        auto [deleteStatus, deleteResponse] = client.request("DELETE",
                                                             "/api/groups/" + std::to_string(groupId));

        REQUIRE(deleteStatus == 200);

        auto [getStatus, getResponse] = client.request("GET", "/api/groups/" + std::to_string(groupId));
        REQUIRE(getStatus == 404);
    }

    server->stop();
    asioContext.stop();
}

TEST_CASE("RestApiServer CORS support", "[RestApi][Integration]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();
    auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8189);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::io_context clientIo;
    TestHttpClient client(clientIo);
    client.setPort(8189);

    SECTION("OPTIONS request returns CORS headers") {
        auto [status, body] = client.request("OPTIONS", "/api/hosts");

        REQUIRE(status == 204);
    }

    server->stop();
    asioContext.stop();
}

TEST_CASE("RestApiServer 404 for unknown endpoints", "[RestApi][Integration]") {
    infra::AsioContext asioContext(2);
    asioContext.start();

    auto db = createTestDatabase();
    auto server = std::make_shared<infra::RestApiServer>(asioContext, db, 8190);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::io_context clientIo;
    TestHttpClient client(clientIo);
    client.setPort(8190);

    SECTION("Unknown endpoint returns 404") {
        auto [status, body] = client.request("GET", "/api/unknown");

        REQUIRE(status == 404);
        auto response = nlohmann::json::parse(body);
        REQUIRE(response["error"].get<std::string>().find("not found") != std::string::npos);
    }

    server->stop();
    asioContext.stop();
}
