#include "infrastructure/api/RestApiServer.hpp"

#include <algorithm>
#include <chrono>
#include <regex>
#include <spdlog/spdlog.h>
#include <sstream>

namespace netpulse::infra {

namespace {

std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    auto end = str.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

nlohmann::json hostToJson(const core::Host& host) {
    nlohmann::json j;
    j["id"] = host.id;
    j["name"] = host.name;
    j["address"] = host.address;
    j["pingIntervalSeconds"] = host.pingIntervalSeconds;
    j["warningThresholdMs"] = host.warningThresholdMs;
    j["criticalThresholdMs"] = host.criticalThresholdMs;
    j["status"] = host.statusToString();
    j["enabled"] = host.enabled;
    if (host.groupId) {
        j["groupId"] = *host.groupId;
    } else {
        j["groupId"] = nullptr;
    }
    j["createdAt"] = std::chrono::duration_cast<std::chrono::seconds>(
                         host.createdAt.time_since_epoch())
                         .count();
    if (host.lastChecked) {
        j["lastChecked"] = std::chrono::duration_cast<std::chrono::seconds>(
                               host.lastChecked->time_since_epoch())
                               .count();
    } else {
        j["lastChecked"] = nullptr;
    }
    return j;
}

nlohmann::json groupToJson(const core::HostGroup& group) {
    nlohmann::json j;
    j["id"] = group.id;
    j["name"] = group.name;
    j["description"] = group.description;
    if (group.parentId) {
        j["parentId"] = *group.parentId;
    } else {
        j["parentId"] = nullptr;
    }
    j["createdAt"] = std::chrono::duration_cast<std::chrono::seconds>(
                         group.createdAt.time_since_epoch())
                         .count();
    return j;
}

nlohmann::json alertToJson(const core::Alert& alert) {
    nlohmann::json j;
    j["id"] = alert.id;
    j["hostId"] = alert.hostId;
    j["type"] = alert.typeToString();
    j["severity"] = alert.severityToString();
    j["title"] = alert.title;
    j["message"] = alert.message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                         alert.timestamp.time_since_epoch())
                         .count();
    j["acknowledged"] = alert.acknowledged;
    return j;
}

nlohmann::json pingResultToJson(const core::PingResult& result) {
    nlohmann::json j;
    j["id"] = result.id;
    j["hostId"] = result.hostId;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                         result.timestamp.time_since_epoch())
                         .count();
    j["latencyMs"] = result.latencyMs();
    j["success"] = result.success;
    if (result.ttl) {
        j["ttl"] = *result.ttl;
    } else {
        j["ttl"] = nullptr;
    }
    j["errorMessage"] = result.errorMessage;
    return j;
}

nlohmann::json statisticsToJson(const core::PingStatistics& stats) {
    nlohmann::json j;
    j["hostId"] = stats.hostId;
    j["totalPings"] = stats.totalPings;
    j["successfulPings"] = stats.successfulPings;
    j["minLatencyMs"] = static_cast<double>(stats.minLatency.count()) / 1000.0;
    j["maxLatencyMs"] = static_cast<double>(stats.maxLatency.count()) / 1000.0;
    j["avgLatencyMs"] = static_cast<double>(stats.avgLatency.count()) / 1000.0;
    j["jitterMs"] = static_cast<double>(stats.jitter.count()) / 1000.0;
    j["packetLossPercent"] = stats.packetLossPercent;
    j["successRate"] = stats.successRate();
    return j;
}

nlohmann::json portScanToJson(const core::PortScanResult& scan) {
    nlohmann::json j;
    j["id"] = scan.id;
    j["targetAddress"] = scan.targetAddress;
    j["port"] = scan.port;
    j["state"] = scan.stateToString();
    j["serviceName"] = scan.serviceName;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                         scan.scanTimestamp.time_since_epoch())
                         .count();
    return j;
}

} // namespace

void ApiResponse::setJson(const nlohmann::json& json) {
    body = json.dump();
    headers["Content-Type"] = "application/json";
}

void ApiResponse::setError(int code, const std::string& message) {
    statusCode = code;
    switch (code) {
    case 400:
        statusText = "Bad Request";
        break;
    case 401:
        statusText = "Unauthorized";
        break;
    case 403:
        statusText = "Forbidden";
        break;
    case 404:
        statusText = "Not Found";
        break;
    case 405:
        statusText = "Method Not Allowed";
        break;
    case 500:
        statusText = "Internal Server Error";
        break;
    default:
        statusText = "Error";
    }
    nlohmann::json error;
    error["error"] = message;
    error["status"] = code;
    setJson(error);
}

std::string ApiResponse::toString() const {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    for (const auto& [key, value] : headers) {
        ss << key << ": " << value << "\r\n";
    }
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: close\r\n";
    ss << "\r\n";
    ss << body;
    return ss.str();
}

RestApiServer::RestApiServer(AsioContext& asioContext, std::shared_ptr<Database> database,
                             uint16_t port)
    : asioContext_(asioContext), database_(std::move(database)), port_(port) {
    hostRepo_ = std::make_unique<HostRepository>(database_);
    groupRepo_ = std::make_unique<HostGroupRepository>(database_);
    metricsRepo_ = std::make_unique<MetricsRepository>(database_);
    registerRoutes();
}

RestApiServer::~RestApiServer() {
    stop();
}

void RestApiServer::registerRoutes() {
    // Health endpoint (no auth required)
    routes_.push_back(
        {HttpMethod::GET, "/api/health", [this](auto& req, auto& res) { handleHealth(req, res); },
         false});

    // Host endpoints
    routes_.push_back(
        {HttpMethod::GET, "/api/hosts", [this](auto& req, auto& res) { handleGetHosts(req, res); }});
    routes_.push_back({HttpMethod::GET, "/api/hosts/:id",
                       [this](auto& req, auto& res) { handleGetHost(req, res); }});
    routes_.push_back({HttpMethod::POST, "/api/hosts",
                       [this](auto& req, auto& res) { handleCreateHost(req, res); }});
    routes_.push_back({HttpMethod::PUT, "/api/hosts/:id",
                       [this](auto& req, auto& res) { handleUpdateHost(req, res); }});
    routes_.push_back({HttpMethod::DELETE, "/api/hosts/:id",
                       [this](auto& req, auto& res) { handleDeleteHost(req, res); }});

    // Group endpoints
    routes_.push_back({HttpMethod::GET, "/api/groups",
                       [this](auto& req, auto& res) { handleGetGroups(req, res); }});
    routes_.push_back({HttpMethod::GET, "/api/groups/:id",
                       [this](auto& req, auto& res) { handleGetGroup(req, res); }});
    routes_.push_back({HttpMethod::POST, "/api/groups",
                       [this](auto& req, auto& res) { handleCreateGroup(req, res); }});
    routes_.push_back({HttpMethod::DELETE, "/api/groups/:id",
                       [this](auto& req, auto& res) { handleDeleteGroup(req, res); }});

    // Alert endpoints
    routes_.push_back({HttpMethod::GET, "/api/alerts",
                       [this](auto& req, auto& res) { handleGetAlerts(req, res); }});
    routes_.push_back({HttpMethod::POST, "/api/alerts/:id/acknowledge",
                       [this](auto& req, auto& res) { handleAcknowledgeAlert(req, res); }});
    routes_.push_back({HttpMethod::POST, "/api/alerts/acknowledge-all",
                       [this](auto& req, auto& res) { handleAcknowledgeAll(req, res); }});

    // Metrics endpoints
    routes_.push_back({HttpMethod::GET, "/api/hosts/:id/metrics",
                       [this](auto& req, auto& res) { handleGetHostMetrics(req, res); }});
    routes_.push_back({HttpMethod::GET, "/api/hosts/:id/statistics",
                       [this](auto& req, auto& res) { handleGetHostStatistics(req, res); }});
    routes_.push_back({HttpMethod::GET, "/api/hosts/:id/export",
                       [this](auto& req, auto& res) { handleExportMetrics(req, res); }});

    // Port scan endpoints
    routes_.push_back({HttpMethod::GET, "/api/portscans",
                       [this](auto& req, auto& res) { handleGetPortScans(req, res); }});
}

void RestApiServer::start() {
    if (running_.load()) {
        return;
    }

    try {
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            asioContext_.getContext(),
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port_));

        running_ = true;
        startAccept();
        spdlog::info("REST API server started on port {}", port_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to start REST API server: {}", e.what());
        throw;
    }
}

void RestApiServer::stop() {
    if (!running_.load()) {
        return;
    }

    running_ = false;
    if (acceptor_) {
        asio::error_code ec;
        acceptor_->close(ec);
        acceptor_.reset();
    }
    spdlog::info("REST API server stopped");
}

void RestApiServer::startAccept() {
    if (!running_.load()) {
        return;
    }

    auto socket = std::make_shared<asio::ip::tcp::socket>(asioContext_.getContext());
    auto self = shared_from_this();

    acceptor_->async_accept(*socket, [this, self, socket](const asio::error_code& ec) {
        if (!ec && running_.load()) {
            handleConnection(socket);
        }
        if (running_.load()) {
            startAccept();
        }
    });
}

void RestApiServer::handleConnection(std::shared_ptr<asio::ip::tcp::socket> socket) {
    readRequest(socket);
}

void RestApiServer::readRequest(std::shared_ptr<asio::ip::tcp::socket> socket) {
    auto buffer = std::make_shared<asio::streambuf>();
    auto self = shared_from_this();

    asio::async_read_until(
        *socket, *buffer, "\r\n\r\n",
        [this, self, socket, buffer](const asio::error_code& ec, std::size_t /*bytesTransferred*/) {
            if (ec) {
                return;
            }

            std::string headerData((std::istreambuf_iterator<char>(&*buffer)),
                                   std::istreambuf_iterator<char>());

            // Check for Content-Length header
            size_t contentLength = 0;
            std::istringstream iss(headerData);
            std::string line;
            while (std::getline(iss, line) && line != "\r") {
                if (line.find("Content-Length:") == 0 || line.find("content-length:") == 0) {
                    auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        contentLength = std::stoull(trim(line.substr(pos + 1)));
                    }
                }
            }

            if (contentLength > 0) {
                // Find where headers end
                auto headerEnd = headerData.find("\r\n\r\n");
                size_t bodyInBuffer = (headerEnd != std::string::npos)
                                          ? headerData.size() - headerEnd - 4
                                          : 0;
                size_t remaining = contentLength > bodyInBuffer ? contentLength - bodyInBuffer : 0;

                if (remaining > 0) {
                    auto bodyBuffer = std::make_shared<std::vector<char>>(remaining);
                    asio::async_read(
                        *socket, asio::buffer(*bodyBuffer),
                        [this, self, socket, headerData, bodyBuffer](const asio::error_code& ec2,
                                                                     std::size_t /*bytes*/) {
                            if (!ec2) {
                                std::string fullRequest =
                                    headerData + std::string(bodyBuffer->begin(), bodyBuffer->end());
                                processRequest(socket, fullRequest);
                            }
                        });
                    return;
                }
            }

            processRequest(socket, headerData);
        });
}

void RestApiServer::processRequest(std::shared_ptr<asio::ip::tcp::socket> socket,
                                   const std::string& rawRequest) {
    ApiRequest request = parseRequest(rawRequest);
    ApiResponse response;
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, X-API-Key, Authorization";

    // Handle CORS preflight
    if (request.method == HttpMethod::OPTIONS) {
        response.statusCode = 204;
        response.statusText = "No Content";
        sendResponse(socket, response);
        return;
    }

    spdlog::debug("REST API request: {} {}", static_cast<int>(request.method), request.path);

    // Find matching route
    bool routeFound = false;
    for (auto& route : routes_) {
        if (route.method == request.method &&
            matchRoute(route.pattern, request.path, request.pathParams)) {

            // Check authentication
            if (route.requiresAuth && !apiKey_.empty() && !validateApiKey(request)) {
                response.setError(401, "Invalid or missing API key");
                sendResponse(socket, response);
                return;
            }

            try {
                route.handler(request, response);
            } catch (const std::exception& e) {
                spdlog::error("REST API error: {}", e.what());
                response.setError(500, "Internal server error");
            }
            routeFound = true;
            break;
        }
    }

    if (!routeFound) {
        response.setError(404, "Endpoint not found");
    }

    sendResponse(socket, response);
}

void RestApiServer::sendResponse(std::shared_ptr<asio::ip::tcp::socket> socket,
                                 const ApiResponse& response) {
    auto responseStr = std::make_shared<std::string>(response.toString());
    auto self = shared_from_this();

    asio::async_write(*socket, asio::buffer(*responseStr),
                      [socket, responseStr](const asio::error_code& /*ec*/, std::size_t /*bytes*/) {
                          asio::error_code shutdownEc;
                          socket->shutdown(asio::ip::tcp::socket::shutdown_both, shutdownEc);
                      });
}

ApiRequest RestApiServer::parseRequest(const std::string& rawRequest) {
    ApiRequest request;
    std::istringstream iss(rawRequest);
    std::string line;

    // Parse request line
    if (std::getline(iss, line)) {
        std::istringstream lineStream(trim(line));
        std::string method, path, version;
        lineStream >> method >> path >> version;

        request.method = parseMethod(method);

        // Parse query string
        auto queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            request.queryParams = parseQueryString(path.substr(queryPos + 1));
            path = path.substr(0, queryPos);
        }
        request.path = path;
    }

    // Parse headers
    while (std::getline(iss, line) && line != "\r" && !line.empty()) {
        auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = trim(line.substr(0, colonPos));
            std::string value = trim(line.substr(colonPos + 1));
            // Normalize header key to lowercase
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            request.headers[key] = value;
        }
    }

    // Parse body (everything after \r\n\r\n)
    auto bodyStart = rawRequest.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        request.body = rawRequest.substr(bodyStart + 4);
    }

    return request;
}

HttpMethod RestApiServer::parseMethod(const std::string& method) {
    if (method == "GET")
        return HttpMethod::GET;
    if (method == "POST")
        return HttpMethod::POST;
    if (method == "PUT")
        return HttpMethod::PUT;
    if (method == "DELETE")
        return HttpMethod::DELETE;
    if (method == "OPTIONS")
        return HttpMethod::OPTIONS;
    return HttpMethod::UNKNOWN;
}

std::map<std::string, std::string> RestApiServer::parseQueryString(const std::string& queryString) {
    std::map<std::string, std::string> params;
    std::istringstream iss(queryString);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        auto eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            params[pair.substr(0, eqPos)] = pair.substr(eqPos + 1);
        }
    }

    return params;
}

bool RestApiServer::matchRoute(const std::string& pattern, const std::string& path,
                               std::map<std::string, std::string>& pathParams) {
    pathParams.clear();

    std::vector<std::string> patternParts, pathParts;
    std::istringstream patternStream(pattern), pathStream(path);
    std::string part;

    while (std::getline(patternStream, part, '/')) {
        if (!part.empty())
            patternParts.push_back(part);
    }
    while (std::getline(pathStream, part, '/')) {
        if (!part.empty())
            pathParts.push_back(part);
    }

    if (patternParts.size() != pathParts.size()) {
        return false;
    }

    for (size_t i = 0; i < patternParts.size(); ++i) {
        if (patternParts[i].front() == ':') {
            // This is a path parameter
            pathParams[patternParts[i].substr(1)] = pathParts[i];
        } else if (patternParts[i] != pathParts[i]) {
            return false;
        }
    }

    return true;
}

bool RestApiServer::validateApiKey(const ApiRequest& request) {
    // Check X-API-Key header
    auto it = request.headers.find("x-api-key");
    if (it != request.headers.end() && it->second == apiKey_) {
        return true;
    }

    // Check Authorization header (Bearer token)
    it = request.headers.find("authorization");
    if (it != request.headers.end()) {
        std::string auth = it->second;
        if (auth.find("Bearer ") == 0) {
            std::string token = auth.substr(7);
            if (token == apiKey_) {
                return true;
            }
        }
    }

    // Check query parameter
    auto qit = request.queryParams.find("api_key");
    if (qit != request.queryParams.end() && qit->second == apiKey_) {
        return true;
    }

    return false;
}

// Host endpoints
void RestApiServer::handleGetHosts(const ApiRequest& /*req*/, ApiResponse& res) {
    auto hosts = hostRepo_->findAll();
    nlohmann::json result = nlohmann::json::array();

    for (const auto& host : hosts) {
        result.push_back(hostToJson(host));
    }

    nlohmann::json response;
    response["hosts"] = result;
    response["count"] = hosts.size();
    res.setJson(response);
}

void RestApiServer::handleGetHost(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing host ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto host = hostRepo_->findById(id);

    if (!host) {
        res.setError(404, "Host not found");
        return;
    }

    res.setJson(hostToJson(*host));
}

void RestApiServer::handleCreateHost(const ApiRequest& req, ApiResponse& res) {
    try {
        auto json = nlohmann::json::parse(req.body);

        core::Host host;
        host.name = json.value("name", "");
        host.address = json.value("address", "");
        host.pingIntervalSeconds = json.value("pingIntervalSeconds", 30);
        host.warningThresholdMs = json.value("warningThresholdMs", 100);
        host.criticalThresholdMs = json.value("criticalThresholdMs", 500);
        host.enabled = json.value("enabled", true);

        if (json.contains("groupId") && !json["groupId"].is_null()) {
            host.groupId = json["groupId"].get<int64_t>();
        }

        if (!host.isValid()) {
            res.setError(400, "Invalid host data: name and address are required");
            return;
        }

        host.id = hostRepo_->insert(host);
        host.createdAt = std::chrono::system_clock::now();

        nlohmann::json response;
        response["host"] = hostToJson(host);
        response["message"] = "Host created successfully";
        res.statusCode = 201;
        res.statusText = "Created";
        res.setJson(response);

        spdlog::info("REST API: Created host '{}' (id: {})", host.name, host.id);
    } catch (const nlohmann::json::exception& e) {
        res.setError(400, std::string("Invalid JSON: ") + e.what());
    }
}

void RestApiServer::handleUpdateHost(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing host ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto existingHost = hostRepo_->findById(id);

    if (!existingHost) {
        res.setError(404, "Host not found");
        return;
    }

    try {
        auto json = nlohmann::json::parse(req.body);

        core::Host host = *existingHost;
        if (json.contains("name"))
            host.name = json["name"];
        if (json.contains("address"))
            host.address = json["address"];
        if (json.contains("pingIntervalSeconds"))
            host.pingIntervalSeconds = json["pingIntervalSeconds"];
        if (json.contains("warningThresholdMs"))
            host.warningThresholdMs = json["warningThresholdMs"];
        if (json.contains("criticalThresholdMs"))
            host.criticalThresholdMs = json["criticalThresholdMs"];
        if (json.contains("enabled"))
            host.enabled = json["enabled"];
        if (json.contains("groupId")) {
            if (json["groupId"].is_null()) {
                host.groupId = std::nullopt;
            } else {
                host.groupId = json["groupId"].get<int64_t>();
            }
        }

        if (!host.isValid()) {
            res.setError(400, "Invalid host data");
            return;
        }

        hostRepo_->update(host);

        nlohmann::json response;
        response["host"] = hostToJson(host);
        response["message"] = "Host updated successfully";
        res.setJson(response);

        spdlog::info("REST API: Updated host '{}' (id: {})", host.name, host.id);
    } catch (const nlohmann::json::exception& e) {
        res.setError(400, std::string("Invalid JSON: ") + e.what());
    }
}

void RestApiServer::handleDeleteHost(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing host ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto host = hostRepo_->findById(id);

    if (!host) {
        res.setError(404, "Host not found");
        return;
    }

    hostRepo_->remove(id);

    nlohmann::json response;
    response["message"] = "Host deleted successfully";
    res.setJson(response);

    spdlog::info("REST API: Deleted host id: {}", id);
}

// Group endpoints
void RestApiServer::handleGetGroups(const ApiRequest& /*req*/, ApiResponse& res) {
    auto groups = groupRepo_->findAll();
    nlohmann::json result = nlohmann::json::array();

    for (const auto& group : groups) {
        result.push_back(groupToJson(group));
    }

    nlohmann::json response;
    response["groups"] = result;
    response["count"] = groups.size();
    res.setJson(response);
}

void RestApiServer::handleGetGroup(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing group ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto group = groupRepo_->findById(id);

    if (!group) {
        res.setError(404, "Group not found");
        return;
    }

    auto groupJson = groupToJson(*group);

    // Include hosts in this group
    auto hosts = hostRepo_->findByGroupId(id);
    nlohmann::json hostsArray = nlohmann::json::array();
    for (const auto& host : hosts) {
        hostsArray.push_back(hostToJson(host));
    }
    groupJson["hosts"] = hostsArray;

    res.setJson(groupJson);
}

void RestApiServer::handleCreateGroup(const ApiRequest& req, ApiResponse& res) {
    try {
        auto json = nlohmann::json::parse(req.body);

        core::HostGroup group;
        group.name = json.value("name", "");
        group.description = json.value("description", "");

        if (json.contains("parentId") && !json["parentId"].is_null()) {
            group.parentId = json["parentId"].get<int64_t>();
        }

        if (!group.isValid()) {
            res.setError(400, "Invalid group data: name is required");
            return;
        }

        group.id = groupRepo_->insert(group);
        group.createdAt = std::chrono::system_clock::now();

        nlohmann::json response;
        response["group"] = groupToJson(group);
        response["message"] = "Group created successfully";
        res.statusCode = 201;
        res.statusText = "Created";
        res.setJson(response);

        spdlog::info("REST API: Created group '{}' (id: {})", group.name, group.id);
    } catch (const nlohmann::json::exception& e) {
        res.setError(400, std::string("Invalid JSON: ") + e.what());
    }
}

void RestApiServer::handleDeleteGroup(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing group ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto group = groupRepo_->findById(id);

    if (!group) {
        res.setError(404, "Group not found");
        return;
    }

    groupRepo_->remove(id);

    nlohmann::json response;
    response["message"] = "Group deleted successfully";
    res.setJson(response);

    spdlog::info("REST API: Deleted group id: {}", id);
}

// Alert endpoints
void RestApiServer::handleGetAlerts(const ApiRequest& req, ApiResponse& res) {
    int limit = 100;
    auto limitIt = req.queryParams.find("limit");
    if (limitIt != req.queryParams.end()) {
        limit = std::stoi(limitIt->second);
    }

    core::AlertFilter filter;
    auto severityIt = req.queryParams.find("severity");
    if (severityIt != req.queryParams.end()) {
        filter.severity = core::Alert::severityFromString(severityIt->second);
    }

    auto typeIt = req.queryParams.find("type");
    if (typeIt != req.queryParams.end()) {
        filter.type = core::Alert::typeFromString(typeIt->second);
    }

    auto ackIt = req.queryParams.find("acknowledged");
    if (ackIt != req.queryParams.end()) {
        filter.acknowledged = (ackIt->second == "true" || ackIt->second == "1");
    }

    auto searchIt = req.queryParams.find("search");
    if (searchIt != req.queryParams.end()) {
        filter.searchText = searchIt->second;
    }

    std::vector<core::Alert> alerts;
    if (filter.isEmpty()) {
        alerts = metricsRepo_->getAlerts(limit);
    } else {
        alerts = metricsRepo_->getAlertsFiltered(filter, limit);
    }

    nlohmann::json result = nlohmann::json::array();
    for (const auto& alert : alerts) {
        result.push_back(alertToJson(alert));
    }

    nlohmann::json response;
    response["alerts"] = result;
    response["count"] = alerts.size();
    res.setJson(response);
}

void RestApiServer::handleAcknowledgeAlert(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing alert ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    metricsRepo_->acknowledgeAlert(id);

    nlohmann::json response;
    response["message"] = "Alert acknowledged";
    res.setJson(response);

    spdlog::info("REST API: Acknowledged alert id: {}", id);
}

void RestApiServer::handleAcknowledgeAll(const ApiRequest& /*req*/, ApiResponse& res) {
    metricsRepo_->acknowledgeAll();

    nlohmann::json response;
    response["message"] = "All alerts acknowledged";
    res.setJson(response);

    spdlog::info("REST API: Acknowledged all alerts");
}

// Metrics endpoints
void RestApiServer::handleGetHostMetrics(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing host ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto host = hostRepo_->findById(id);

    if (!host) {
        res.setError(404, "Host not found");
        return;
    }

    int limit = 100;
    auto limitIt = req.queryParams.find("limit");
    if (limitIt != req.queryParams.end()) {
        limit = std::stoi(limitIt->second);
    }

    auto results = metricsRepo_->getPingResults(id, limit);
    nlohmann::json pingResults = nlohmann::json::array();
    for (const auto& result : results) {
        pingResults.push_back(pingResultToJson(result));
    }

    nlohmann::json response;
    response["hostId"] = id;
    response["hostName"] = host->name;
    response["results"] = pingResults;
    response["count"] = results.size();
    res.setJson(response);
}

void RestApiServer::handleGetHostStatistics(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing host ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto host = hostRepo_->findById(id);

    if (!host) {
        res.setError(404, "Host not found");
        return;
    }

    int sampleCount = 100;
    auto sampleIt = req.queryParams.find("samples");
    if (sampleIt != req.queryParams.end()) {
        sampleCount = std::stoi(sampleIt->second);
    }

    auto stats = metricsRepo_->getStatistics(id, sampleCount);
    auto statsJson = statisticsToJson(stats);
    statsJson["hostName"] = host->name;
    statsJson["hostAddress"] = host->address;
    statsJson["hostStatus"] = host->statusToString();

    res.setJson(statsJson);
}

void RestApiServer::handleExportMetrics(const ApiRequest& req, ApiResponse& res) {
    auto idIt = req.pathParams.find("id");
    if (idIt == req.pathParams.end()) {
        res.setError(400, "Missing host ID");
        return;
    }

    int64_t id = std::stoll(idIt->second);
    auto host = hostRepo_->findById(id);

    if (!host) {
        res.setError(404, "Host not found");
        return;
    }

    std::string format = "json";
    auto formatIt = req.queryParams.find("format");
    if (formatIt != req.queryParams.end()) {
        format = formatIt->second;
    }

    if (format == "csv") {
        res.body = metricsRepo_->exportToCsv(id);
        res.headers["Content-Type"] = "text/csv";
        res.headers["Content-Disposition"] =
            "attachment; filename=\"metrics_" + std::to_string(id) + ".csv\"";
    } else {
        res.body = metricsRepo_->exportToJson(id);
        res.headers["Content-Type"] = "application/json";
    }
}

// Port scan endpoints
void RestApiServer::handleGetPortScans(const ApiRequest& req, ApiResponse& res) {
    auto addressIt = req.queryParams.find("address");
    if (addressIt == req.queryParams.end()) {
        res.setError(400, "Missing 'address' query parameter");
        return;
    }

    int limit = 1000;
    auto limitIt = req.queryParams.find("limit");
    if (limitIt != req.queryParams.end()) {
        limit = std::stoi(limitIt->second);
    }

    auto results = metricsRepo_->getPortScanResults(addressIt->second, limit);
    nlohmann::json scanResults = nlohmann::json::array();
    for (const auto& result : results) {
        scanResults.push_back(portScanToJson(result));
    }

    nlohmann::json response;
    response["address"] = addressIt->second;
    response["results"] = scanResults;
    response["count"] = results.size();
    res.setJson(response);
}

// Health endpoint
void RestApiServer::handleHealth(const ApiRequest& /*req*/, ApiResponse& res) {
    nlohmann::json health;
    health["status"] = "healthy";
    health["timestamp"] =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    health["version"] = "1.0.0";

    int hostCount = hostRepo_->count();
    health["hosts"] = hostCount;

    res.setJson(health);
}

} // namespace netpulse::infra
