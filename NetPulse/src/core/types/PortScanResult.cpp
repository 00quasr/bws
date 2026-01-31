#include "core/types/PortScanResult.hpp"

namespace netpulse::core {

std::string PortScanResult::stateToString() const {
    switch (state) {
    case PortState::Unknown:
        return "Unknown";
    case PortState::Open:
        return "Open";
    case PortState::Closed:
        return "Closed";
    case PortState::Filtered:
        return "Filtered";
    }
    return "Unknown";
}

PortState PortScanResult::stateFromString(const std::string& str) {
    if (str == "Open")
        return PortState::Open;
    if (str == "Closed")
        return PortState::Closed;
    if (str == "Filtered")
        return PortState::Filtered;
    return PortState::Unknown;
}

std::vector<uint16_t> PortScanConfig::getPortsToScan() const {
    switch (range) {
    case PortRange::Common:
        return {20,   21,   22,   23,   25,   53,    80,   110,  111,  135,
                139,  143,  443,  445,  993,  995,   1723, 3306, 3389, 5432,
                5900, 8080, 8443, 8888, 9090, 27017, 6379, 11211};
    case PortRange::Web:
        return {80, 443, 8080, 8443, 8000, 8888, 3000, 5000, 9000, 9090};
    case PortRange::Database:
        return {3306, 5432, 1433, 1521, 27017, 6379, 11211, 5984, 9200, 7474};
    case PortRange::All: {
        std::vector<uint16_t> ports;
        ports.reserve(65535);
        for (int p = 1; p <= 65535; ++p) {
            ports.push_back(static_cast<uint16_t>(p));
        }
        return ports;
    }
    case PortRange::Custom:
        return customPorts;
    }
    return {};
}

const std::unordered_map<uint16_t, std::string>& ServiceDetector::getKnownServices() {
    static const std::unordered_map<uint16_t, std::string> services = {
        {20, "ftp-data"},   {21, "ftp"},        {22, "ssh"},        {23, "telnet"},
        {25, "smtp"},       {53, "dns"},        {80, "http"},       {110, "pop3"},
        {111, "rpcbind"},   {135, "msrpc"},     {139, "netbios"},   {143, "imap"},
        {443, "https"},     {445, "smb"},       {993, "imaps"},     {995, "pop3s"},
        {1433, "mssql"},    {1521, "oracle"},   {1723, "pptp"},     {3306, "mysql"},
        {3389, "rdp"},      {5432, "postgres"}, {5900, "vnc"},      {5984, "couchdb"},
        {6379, "redis"},    {7474, "neo4j"},    {8080, "http-alt"}, {8443, "https-alt"},
        {8888, "http-alt"}, {9000, "http-alt"}, {9090, "http-alt"}, {9200, "elasticsearch"},
        {11211, "memcached"}, {27017, "mongodb"}};
    return services;
}

std::string ServiceDetector::detectService(uint16_t port) {
    const auto& services = getKnownServices();
    auto it = services.find(port);
    return it != services.end() ? it->second : "";
}

} // namespace netpulse::core
