#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace netpulse::core {

enum class SnmpVersion : int { V1 = 1, V2c = 2, V3 = 3 };

enum class SnmpAuthProtocol : int { None = 0, MD5 = 1, SHA = 2, SHA256 = 3 };

enum class SnmpPrivProtocol : int { None = 0, DES = 1, AES128 = 2, AES256 = 3 };

enum class SnmpSecurityLevel : int {
    NoAuthNoPriv = 1,  // No authentication, no privacy
    AuthNoPriv = 2,    // Authentication, no privacy
    AuthPriv = 3       // Authentication and privacy
};

// SNMP v2c credentials (community string)
struct SnmpV2cCredentials {
    std::string community{"public"};

    bool operator==(const SnmpV2cCredentials& other) const = default;
};

// SNMP v3 credentials (USM - User-based Security Model)
struct SnmpV3Credentials {
    std::string username;
    SnmpSecurityLevel securityLevel{SnmpSecurityLevel::NoAuthNoPriv};
    SnmpAuthProtocol authProtocol{SnmpAuthProtocol::None};
    std::string authPassword;
    SnmpPrivProtocol privProtocol{SnmpPrivProtocol::None};
    std::string privPassword;
    std::string contextName;
    std::string contextEngineId;

    bool operator==(const SnmpV3Credentials& other) const = default;
};

// Union of credentials for different SNMP versions
using SnmpCredentials = std::variant<SnmpV2cCredentials, SnmpV3Credentials>;

// SNMP data types
enum class SnmpDataType : int {
    Integer = 0,
    OctetString = 1,
    ObjectIdentifier = 2,
    IpAddress = 3,
    Counter32 = 4,
    Gauge32 = 5,
    TimeTicks = 6,
    Counter64 = 7,
    Null = 8,
    NoSuchObject = 9,
    NoSuchInstance = 10,
    EndOfMibView = 11,
    Unknown = 99
};

// Represents a single SNMP variable binding (OID + value)
struct SnmpVarBind {
    std::string oid;
    SnmpDataType type{SnmpDataType::Unknown};
    std::string value;  // String representation of the value
    std::optional<int64_t> intValue;
    std::optional<uint64_t> counterValue;

    bool operator==(const SnmpVarBind& other) const = default;
};

// Result of a single SNMP query
struct SnmpResult {
    int64_t id{0};
    int64_t hostId{0};
    std::chrono::system_clock::time_point timestamp;
    SnmpVersion version{SnmpVersion::V2c};
    std::vector<SnmpVarBind> varbinds;
    std::chrono::microseconds responseTime{0};
    bool success{false};
    std::string errorMessage;
    int errorStatus{0};  // SNMP error status (0 = noError)
    int errorIndex{0};   // Index of varbind that caused error

    [[nodiscard]] double responseTimeMs() const {
        return static_cast<double>(responseTime.count()) / 1000.0;
    }

    // Get a specific OID value (convenience method)
    [[nodiscard]] std::optional<SnmpVarBind> getVarBind(const std::string& oid) const {
        for (const auto& vb : varbinds) {
            if (vb.oid == oid) {
                return vb;
            }
        }
        return std::nullopt;
    }

    bool operator==(const SnmpResult& other) const = default;
};

// Configuration for SNMP device monitoring
struct SnmpDeviceConfig {
    int64_t id{0};
    int64_t hostId{0};  // Foreign key to Host
    SnmpVersion version{SnmpVersion::V2c};
    SnmpCredentials credentials{SnmpV2cCredentials{}};
    uint16_t port{161};
    int timeoutMs{5000};
    int retries{1};
    int pollIntervalSeconds{60};
    std::vector<std::string> oids;  // OIDs to monitor
    bool enabled{true};
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> lastPolled;

    bool operator==(const SnmpDeviceConfig& other) const = default;
};

// Common SNMP OIDs
namespace SnmpOids {
    // System MIB (SNMPv2-MIB)
    constexpr const char* SYS_DESCR = "1.3.6.1.2.1.1.1.0";
    constexpr const char* SYS_OBJECT_ID = "1.3.6.1.2.1.1.2.0";
    constexpr const char* SYS_UPTIME = "1.3.6.1.2.1.1.3.0";
    constexpr const char* SYS_CONTACT = "1.3.6.1.2.1.1.4.0";
    constexpr const char* SYS_NAME = "1.3.6.1.2.1.1.5.0";
    constexpr const char* SYS_LOCATION = "1.3.6.1.2.1.1.6.0";
    constexpr const char* SYS_SERVICES = "1.3.6.1.2.1.1.7.0";

    // Interface MIB (IF-MIB)
    constexpr const char* IF_NUMBER = "1.3.6.1.2.1.2.1.0";
    constexpr const char* IF_TABLE = "1.3.6.1.2.1.2.2";
    constexpr const char* IF_DESCR = "1.3.6.1.2.1.2.2.1.2";
    constexpr const char* IF_TYPE = "1.3.6.1.2.1.2.2.1.3";
    constexpr const char* IF_SPEED = "1.3.6.1.2.1.2.2.1.5";
    constexpr const char* IF_ADMIN_STATUS = "1.3.6.1.2.1.2.2.1.7";
    constexpr const char* IF_OPER_STATUS = "1.3.6.1.2.1.2.2.1.8";
    constexpr const char* IF_IN_OCTETS = "1.3.6.1.2.1.2.2.1.10";
    constexpr const char* IF_OUT_OCTETS = "1.3.6.1.2.1.2.2.1.16";
    constexpr const char* IF_IN_ERRORS = "1.3.6.1.2.1.2.2.1.14";
    constexpr const char* IF_OUT_ERRORS = "1.3.6.1.2.1.2.2.1.20";

    // Host Resources MIB (HOST-RESOURCES-MIB)
    constexpr const char* HR_SYSTEM_UPTIME = "1.3.6.1.2.1.25.1.1.0";
    constexpr const char* HR_STORAGE_TABLE = "1.3.6.1.2.1.25.2.3";
    constexpr const char* HR_PROCESSOR_TABLE = "1.3.6.1.2.1.25.3.3";
    constexpr const char* HR_SW_RUN_TABLE = "1.3.6.1.2.1.25.4.2";
}

// SNMP statistics for a monitored device
struct SnmpStatistics {
    int64_t hostId{0};
    int totalPolls{0};
    int successfulPolls{0};
    std::chrono::microseconds minResponseTime{0};
    std::chrono::microseconds maxResponseTime{0};
    std::chrono::microseconds avgResponseTime{0};
    double successRate{0.0};
    std::map<std::string, std::string> lastValues;  // OID -> last value

    [[nodiscard]] double calculateSuccessRate() const {
        return totalPolls > 0 ? (static_cast<double>(successfulPolls) / totalPolls) * 100.0 : 0.0;
    }
};

// Helper functions
inline std::string snmpVersionToString(SnmpVersion version) {
    switch (version) {
        case SnmpVersion::V1: return "v1";
        case SnmpVersion::V2c: return "v2c";
        case SnmpVersion::V3: return "v3";
    }
    return "unknown";
}

inline SnmpVersion snmpVersionFromString(const std::string& str) {
    if (str == "v1" || str == "1") return SnmpVersion::V1;
    if (str == "v3" || str == "3") return SnmpVersion::V3;
    return SnmpVersion::V2c;  // Default to v2c
}

inline std::string snmpAuthProtocolToString(SnmpAuthProtocol protocol) {
    switch (protocol) {
        case SnmpAuthProtocol::None: return "None";
        case SnmpAuthProtocol::MD5: return "MD5";
        case SnmpAuthProtocol::SHA: return "SHA";
        case SnmpAuthProtocol::SHA256: return "SHA256";
    }
    return "None";
}

inline std::string snmpPrivProtocolToString(SnmpPrivProtocol protocol) {
    switch (protocol) {
        case SnmpPrivProtocol::None: return "None";
        case SnmpPrivProtocol::DES: return "DES";
        case SnmpPrivProtocol::AES128: return "AES128";
        case SnmpPrivProtocol::AES256: return "AES256";
    }
    return "None";
}

inline std::string snmpDataTypeToString(SnmpDataType type) {
    switch (type) {
        case SnmpDataType::Integer: return "INTEGER";
        case SnmpDataType::OctetString: return "OCTET STRING";
        case SnmpDataType::ObjectIdentifier: return "OBJECT IDENTIFIER";
        case SnmpDataType::IpAddress: return "IpAddress";
        case SnmpDataType::Counter32: return "Counter32";
        case SnmpDataType::Gauge32: return "Gauge32";
        case SnmpDataType::TimeTicks: return "TimeTicks";
        case SnmpDataType::Counter64: return "Counter64";
        case SnmpDataType::Null: return "Null";
        case SnmpDataType::NoSuchObject: return "noSuchObject";
        case SnmpDataType::NoSuchInstance: return "noSuchInstance";
        case SnmpDataType::EndOfMibView: return "endOfMibView";
        case SnmpDataType::Unknown: return "Unknown";
    }
    return "Unknown";
}

} // namespace netpulse::core
