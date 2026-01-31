#pragma once

#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostGroupRepository.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace netpulse::infra {

enum class HttpMethod { GET, POST, PUT, DELETE, OPTIONS, UNKNOWN };

struct ApiRequest {
    HttpMethod method{HttpMethod::UNKNOWN};
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> queryParams;
    std::map<std::string, std::string> pathParams;
};

struct ApiResponse {
    int statusCode{200};
    std::string statusText{"OK"};
    std::string body;
    std::map<std::string, std::string> headers;

    void setJson(const nlohmann::json& json);
    void setError(int code, const std::string& message);
    std::string toString() const;
};

using RouteHandler = std::function<void(const ApiRequest&, ApiResponse&)>;

struct Route {
    HttpMethod method;
    std::string pattern;
    RouteHandler handler;
    bool requiresAuth{true};
};

class RestApiServer : public std::enable_shared_from_this<RestApiServer> {
public:
    RestApiServer(AsioContext& asioContext, std::shared_ptr<Database> database, uint16_t port = 8080);
    ~RestApiServer();

    RestApiServer(const RestApiServer&) = delete;
    RestApiServer& operator=(const RestApiServer&) = delete;

    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    void setApiKey(const std::string& apiKey) { apiKey_ = apiKey; }
    uint16_t port() const { return port_; }

private:
    void startAccept();
    void handleConnection(std::shared_ptr<asio::ip::tcp::socket> socket);
    void readRequest(std::shared_ptr<asio::ip::tcp::socket> socket);
    void processRequest(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& rawRequest);
    void sendResponse(std::shared_ptr<asio::ip::tcp::socket> socket, const ApiResponse& response);

    ApiRequest parseRequest(const std::string& rawRequest);
    HttpMethod parseMethod(const std::string& method);
    std::map<std::string, std::string> parseQueryString(const std::string& queryString);
    bool matchRoute(const std::string& pattern, const std::string& path,
                    std::map<std::string, std::string>& pathParams);
    bool validateApiKey(const ApiRequest& request);

    void registerRoutes();

    // Host endpoints
    void handleGetHosts(const ApiRequest& req, ApiResponse& res);
    void handleGetHost(const ApiRequest& req, ApiResponse& res);
    void handleCreateHost(const ApiRequest& req, ApiResponse& res);
    void handleUpdateHost(const ApiRequest& req, ApiResponse& res);
    void handleDeleteHost(const ApiRequest& req, ApiResponse& res);

    // Group endpoints
    void handleGetGroups(const ApiRequest& req, ApiResponse& res);
    void handleGetGroup(const ApiRequest& req, ApiResponse& res);
    void handleCreateGroup(const ApiRequest& req, ApiResponse& res);
    void handleDeleteGroup(const ApiRequest& req, ApiResponse& res);

    // Alert endpoints
    void handleGetAlerts(const ApiRequest& req, ApiResponse& res);
    void handleAcknowledgeAlert(const ApiRequest& req, ApiResponse& res);
    void handleAcknowledgeAll(const ApiRequest& req, ApiResponse& res);

    // Metrics endpoints
    void handleGetHostMetrics(const ApiRequest& req, ApiResponse& res);
    void handleGetHostStatistics(const ApiRequest& req, ApiResponse& res);
    void handleExportMetrics(const ApiRequest& req, ApiResponse& res);

    // Port scan endpoints
    void handleGetPortScans(const ApiRequest& req, ApiResponse& res);

    // Health endpoint
    void handleHealth(const ApiRequest& req, ApiResponse& res);

    AsioContext& asioContext_;
    std::shared_ptr<Database> database_;
    uint16_t port_;
    std::string apiKey_;
    std::atomic<bool> running_{false};

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::vector<Route> routes_;

    std::unique_ptr<HostRepository> hostRepo_;
    std::unique_ptr<HostGroupRepository> groupRepo_;
    std::unique_ptr<MetricsRepository> metricsRepo_;
};

} // namespace netpulse::infra
