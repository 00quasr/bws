/**
 * @file SnmpTypes.hpp
 * @brief SNMP types, credentials, and configuration structures.
 *
 * This file defines all types related to SNMP (Simple Network Management Protocol)
 * monitoring including version-specific credentials, data types, OIDs, and statistics.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace netpulse::core {

/**
 * @brief Supported SNMP protocol versions.
 */
enum class SnmpVersion : int {
    V1 = 1,  ///< SNMP version 1 (community-based, no encryption)
    V2c = 2, ///< SNMP version 2c (community-based with enhanced features)
    V3 = 3   ///< SNMP version 3 (user-based security model)
};

/**
 * @brief Authentication protocols for SNMP v3.
 */
enum class SnmpAuthProtocol : int {
    None = 0,   ///< No authentication
    MD5 = 1,    ///< MD5 hash-based authentication
    SHA = 2,    ///< SHA-1 hash-based authentication
    SHA256 = 3  ///< SHA-256 hash-based authentication
};

/**
 * @brief Privacy (encryption) protocols for SNMP v3.
 */
enum class SnmpPrivProtocol : int {
    None = 0,    ///< No encryption
    DES = 1,     ///< DES encryption
    AES128 = 2,  ///< AES-128 encryption
    AES256 = 3   ///< AES-256 encryption
};

/**
 * @brief Security levels for SNMP v3.
 */
enum class SnmpSecurityLevel : int {
    NoAuthNoPriv = 1, ///< No authentication, no privacy (encryption)
    AuthNoPriv = 2,   ///< Authentication only, no privacy
    AuthPriv = 3      ///< Both authentication and privacy
};

/**
 * @brief SNMP v2c credentials using community string.
 */
struct SnmpV2cCredentials {
    std::string community{"public"}; ///< Community string for authentication

    bool operator==(const SnmpV2cCredentials& other) const = default;
};

/**
 * @brief SNMP v3 credentials using User-based Security Model (USM).
 */
struct SnmpV3Credentials {
    std::string username;            ///< Security name (username)
    SnmpSecurityLevel securityLevel{SnmpSecurityLevel::NoAuthNoPriv}; ///< Security level
    SnmpAuthProtocol authProtocol{SnmpAuthProtocol::None}; ///< Authentication protocol
    std::string authPassword;        ///< Authentication password
    SnmpPrivProtocol privProtocol{SnmpPrivProtocol::None}; ///< Privacy protocol
    std::string privPassword;        ///< Privacy password
    std::string contextName;         ///< SNMP context name
    std::string contextEngineId;     ///< Context engine ID

    bool operator==(const SnmpV3Credentials& other) const = default;
};

/**
 * @brief Union type for SNMP credentials supporting different versions.
 */
using SnmpCredentials = std::variant<SnmpV2cCredentials, SnmpV3Credentials>;

/**
 * @brief SNMP data types as defined in RFC 2578.
 */
enum class SnmpDataType : int {
    Integer = 0,          ///< 32-bit signed integer
    OctetString = 1,      ///< Arbitrary binary or text data
    ObjectIdentifier = 2, ///< Object identifier (OID)
    IpAddress = 3,        ///< 32-bit IPv4 address
    Counter32 = 4,        ///< 32-bit counter (wraps at max)
    Gauge32 = 5,          ///< 32-bit gauge (can increase or decrease)
    TimeTicks = 6,        ///< Hundredths of a second since epoch
    Counter64 = 7,        ///< 64-bit counter
    Null = 8,             ///< Null value
    NoSuchObject = 9,     ///< OID does not exist
    NoSuchInstance = 10,  ///< Instance does not exist
    EndOfMibView = 11,    ///< End of MIB tree reached
    Unknown = 99          ///< Unknown data type
};

/**
 * @brief SNMP variable binding (OID + value pair).
 *
 * Represents a single OID and its associated value from an SNMP response.
 */
struct SnmpVarBind {
    std::string oid;                   ///< Object identifier
    SnmpDataType type{SnmpDataType::Unknown}; ///< Data type of the value
    std::string value;                 ///< String representation of the value
    std::optional<int64_t> intValue;   ///< Integer value (if applicable)
    std::optional<uint64_t> counterValue; ///< Counter value (if applicable)

    bool operator==(const SnmpVarBind& other) const = default;
};

/**
 * @brief Result of an SNMP query operation.
 *
 * Contains the response from an SNMP GET, GET-NEXT, or WALK operation.
 */
struct SnmpResult {
    int64_t id{0};                   ///< Unique identifier for the result
    int64_t hostId{0};               ///< ID of the host that was queried
    std::chrono::system_clock::time_point timestamp; ///< When the query was performed
    SnmpVersion version{SnmpVersion::V2c}; ///< SNMP version used
    std::vector<SnmpVarBind> varbinds; ///< Variable bindings in the response
    std::chrono::microseconds responseTime{0}; ///< Time taken for the query
    bool success{false};             ///< Whether the query succeeded
    std::string errorMessage;        ///< Error message if query failed
    int errorStatus{0};              ///< SNMP error status (0 = noError)
    int errorIndex{0};               ///< Index of varbind that caused error

    /**
     * @brief Converts response time to milliseconds.
     * @return Response time as a floating-point number of milliseconds.
     */
    [[nodiscard]] double responseTimeMs() const {
        return static_cast<double>(responseTime.count()) / 1000.0;
    }

    /**
     * @brief Gets a specific variable binding by OID.
     * @param oid The OID to search for.
     * @return The variable binding if found, std::nullopt otherwise.
     */
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

/**
 * @brief Configuration for SNMP device monitoring.
 *
 * Defines how to connect to and monitor an SNMP-enabled device.
 */
struct SnmpDeviceConfig {
    int64_t id{0};                   ///< Unique identifier for the config
    int64_t hostId{0};               ///< Foreign key to the associated Host
    SnmpVersion version{SnmpVersion::V2c}; ///< SNMP version to use
    SnmpCredentials credentials{SnmpV2cCredentials{}}; ///< Authentication credentials
    uint16_t port{161};              ///< SNMP port (default: 161)
    int timeoutMs{5000};             ///< Query timeout in milliseconds
    int retries{1};                  ///< Number of retry attempts
    int pollIntervalSeconds{60};     ///< Interval between polls in seconds
    std::vector<std::string> oids;   ///< OIDs to monitor
    bool enabled{true};              ///< Whether monitoring is enabled
    std::chrono::system_clock::time_point createdAt; ///< When config was created
    std::optional<std::chrono::system_clock::time_point> lastPolled; ///< Last poll time

    bool operator==(const SnmpDeviceConfig& other) const = default;
};

/**
 * @brief Common SNMP OID constants.
 *
 * Contains frequently used OIDs from standard MIBs.
 */
namespace SnmpOids {
    /** @name System MIB (SNMPv2-MIB)
     *  @{ */
    constexpr const char* SYS_DESCR = "1.3.6.1.2.1.1.1.0";     ///< System description
    constexpr const char* SYS_OBJECT_ID = "1.3.6.1.2.1.1.2.0"; ///< System object ID
    constexpr const char* SYS_UPTIME = "1.3.6.1.2.1.1.3.0";    ///< System uptime
    constexpr const char* SYS_CONTACT = "1.3.6.1.2.1.1.4.0";   ///< System contact
    constexpr const char* SYS_NAME = "1.3.6.1.2.1.1.5.0";      ///< System name
    constexpr const char* SYS_LOCATION = "1.3.6.1.2.1.1.6.0";  ///< System location
    constexpr const char* SYS_SERVICES = "1.3.6.1.2.1.1.7.0";  ///< System services
    /** @} */

    /** @name Interface MIB (IF-MIB)
     *  @{ */
    constexpr const char* IF_NUMBER = "1.3.6.1.2.1.2.1.0";       ///< Number of interfaces
    constexpr const char* IF_TABLE = "1.3.6.1.2.1.2.2";          ///< Interface table
    constexpr const char* IF_DESCR = "1.3.6.1.2.1.2.2.1.2";      ///< Interface description
    constexpr const char* IF_TYPE = "1.3.6.1.2.1.2.2.1.3";       ///< Interface type
    constexpr const char* IF_SPEED = "1.3.6.1.2.1.2.2.1.5";      ///< Interface speed
    constexpr const char* IF_ADMIN_STATUS = "1.3.6.1.2.1.2.2.1.7";  ///< Admin status
    constexpr const char* IF_OPER_STATUS = "1.3.6.1.2.1.2.2.1.8";   ///< Operational status
    constexpr const char* IF_IN_OCTETS = "1.3.6.1.2.1.2.2.1.10";    ///< Octets received
    constexpr const char* IF_OUT_OCTETS = "1.3.6.1.2.1.2.2.1.16";   ///< Octets sent
    constexpr const char* IF_IN_ERRORS = "1.3.6.1.2.1.2.2.1.14";    ///< Receive errors
    constexpr const char* IF_OUT_ERRORS = "1.3.6.1.2.1.2.2.1.20";   ///< Transmit errors
    /** @} */

    /** @name Host Resources MIB (HOST-RESOURCES-MIB)
     *  @{ */
    constexpr const char* HR_SYSTEM_UPTIME = "1.3.6.1.2.1.25.1.1.0"; ///< Host uptime
    constexpr const char* HR_STORAGE_TABLE = "1.3.6.1.2.1.25.2.3";   ///< Storage table
    constexpr const char* HR_PROCESSOR_TABLE = "1.3.6.1.2.1.25.3.3"; ///< Processor table
    constexpr const char* HR_SW_RUN_TABLE = "1.3.6.1.2.1.25.4.2";    ///< Running software table
    /** @} */
}

/**
 * @brief Aggregated SNMP statistics for a monitored device.
 */
struct SnmpStatistics {
    int64_t hostId{0};                ///< ID of the monitored host
    int totalPolls{0};                ///< Total number of poll attempts
    int successfulPolls{0};           ///< Number of successful polls
    std::chrono::microseconds minResponseTime{0}; ///< Minimum response time
    std::chrono::microseconds maxResponseTime{0}; ///< Maximum response time
    std::chrono::microseconds avgResponseTime{0}; ///< Average response time
    double successRate{0.0};          ///< Poll success rate (0-100)
    std::map<std::string, std::string> lastValues; ///< Last value for each OID

    /**
     * @brief Calculates the poll success rate as a percentage.
     * @return Percentage of successful polls (0-100).
     */
    [[nodiscard]] double calculateSuccessRate() const {
        return totalPolls > 0 ? (static_cast<double>(successfulPolls) / totalPolls) * 100.0 : 0.0;
    }
};

/**
 * @brief Converts an SNMP version to its string representation.
 * @param version The SNMP version to convert.
 * @return String representation (e.g., "v1", "v2c", "v3").
 */
inline std::string snmpVersionToString(SnmpVersion version) {
    switch (version) {
        case SnmpVersion::V1: return "v1";
        case SnmpVersion::V2c: return "v2c";
        case SnmpVersion::V3: return "v3";
    }
    return "unknown";
}

/**
 * @brief Parses a string to get the corresponding SNMP version.
 * @param str The string to parse (e.g., "v1", "v2c", "v3").
 * @return The corresponding SnmpVersion (defaults to V2c).
 */
inline SnmpVersion snmpVersionFromString(const std::string& str) {
    if (str == "v1" || str == "1") return SnmpVersion::V1;
    if (str == "v3" || str == "3") return SnmpVersion::V3;
    return SnmpVersion::V2c;
}

/**
 * @brief Converts an authentication protocol to its string representation.
 * @param protocol The authentication protocol to convert.
 * @return String representation (e.g., "None", "MD5", "SHA").
 */
inline std::string snmpAuthProtocolToString(SnmpAuthProtocol protocol) {
    switch (protocol) {
        case SnmpAuthProtocol::None: return "None";
        case SnmpAuthProtocol::MD5: return "MD5";
        case SnmpAuthProtocol::SHA: return "SHA";
        case SnmpAuthProtocol::SHA256: return "SHA256";
    }
    return "None";
}

/**
 * @brief Converts a privacy protocol to its string representation.
 * @param protocol The privacy protocol to convert.
 * @return String representation (e.g., "None", "DES", "AES128").
 */
inline std::string snmpPrivProtocolToString(SnmpPrivProtocol protocol) {
    switch (protocol) {
        case SnmpPrivProtocol::None: return "None";
        case SnmpPrivProtocol::DES: return "DES";
        case SnmpPrivProtocol::AES128: return "AES128";
        case SnmpPrivProtocol::AES256: return "AES256";
    }
    return "None";
}

/**
 * @brief Converts an SNMP data type to its string representation.
 * @param type The data type to convert.
 * @return String representation (e.g., "INTEGER", "OCTET STRING").
 */
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
