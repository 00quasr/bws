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

/**
 * @brief HTTP method enumeration.
 */
enum class HttpMethod { GET, POST, PUT, DELETE, OPTIONS, UNKNOWN };

/**
 * @brief Represents an incoming API request.
 */
struct ApiRequest {
    HttpMethod method{HttpMethod::UNKNOWN};         ///< HTTP method of the request.
    std::string path;                               ///< Request path.
    std::string body;                               ///< Request body content.
    std::map<std::string, std::string> headers;     ///< HTTP headers.
    std::map<std::string, std::string> queryParams; ///< Query string parameters.
    std::map<std::string, std::string> pathParams;  ///< Path parameters from route matching.
};

/**
 * @brief Represents an API response to send.
 */
struct ApiResponse {
    int statusCode{200};                            ///< HTTP status code.
    std::string statusText{"OK"};                   ///< HTTP status text.
    std::string body;                               ///< Response body content.
    std::map<std::string, std::string> headers;     ///< Response headers.

    /**
     * @brief Sets the response body as JSON.
     * @param json JSON object to serialize as body.
     */
    void setJson(const nlohmann::json& json);

    /**
     * @brief Sets an error response.
     * @param code HTTP status code for the error.
     * @param message Error message.
     */
    void setError(int code, const std::string& message);

    /**
     * @brief Converts the response to an HTTP response string.
     * @return Complete HTTP response string.
     */
    std::string toString() const;
};

/**
 * @brief Handler function type for route endpoints.
 */
using RouteHandler = std::function<void(const ApiRequest&, ApiResponse&)>;

/**
 * @brief Route definition for API endpoints.
 */
struct Route {
    HttpMethod method;         ///< HTTP method this route handles.
    std::string pattern;       ///< URL pattern (may include path parameters).
    RouteHandler handler;      ///< Handler function for this route.
    bool requiresAuth{true};   ///< Whether API key authentication is required.
};

/**
 * @brief REST API server for external access to NetPulse data.
 *
 * Provides a JSON-based REST API for managing hosts, groups, alerts,
 * and metrics. Supports API key authentication and CORS.
 *
 * @note This class is non-copyable.
 */
class RestApiServer : public std::enable_shared_from_this<RestApiServer> {
public:
    /**
     * @brief Constructs a RestApiServer.
     * @param asioContext Reference to the AsioContext for async I/O.
     * @param database Shared pointer to the Database.
     * @param port TCP port to listen on (default 8080).
     */
    RestApiServer(AsioContext& asioContext, std::shared_ptr<Database> database, uint16_t port = 8080);

    /**
     * @brief Destructor. Stops the server if running.
     */
    ~RestApiServer();

    RestApiServer(const RestApiServer&) = delete;
    RestApiServer& operator=(const RestApiServer&) = delete;

    /**
     * @brief Starts the API server and begins accepting connections.
     */
    void start();

    /**
     * @brief Stops the API server and closes all connections.
     */
    void stop();

    /**
     * @brief Checks if the server is currently running.
     * @return True if running, false otherwise.
     */
    bool isRunning() const { return running_.load(); }

    /**
     * @brief Sets the API key for authentication.
     * @param apiKey The API key required for authenticated endpoints.
     */
    void setApiKey(const std::string& apiKey) { apiKey_ = apiKey; }

    /**
     * @brief Returns the port the server is listening on.
     * @return TCP port number.
     */
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
