#include <catch2/catch_test_macros.hpp>

#include "core/types/SnmpTypes.hpp"

using namespace netpulse::core;

TEST_CASE("SNMP version string conversion", "[SnmpTypes]") {
    SECTION("snmpVersionToString") {
        REQUIRE(snmpVersionToString(SnmpVersion::V1) == "v1");
        REQUIRE(snmpVersionToString(SnmpVersion::V2c) == "v2c");
        REQUIRE(snmpVersionToString(SnmpVersion::V3) == "v3");
    }

    SECTION("snmpVersionFromString") {
        REQUIRE(snmpVersionFromString("v1") == SnmpVersion::V1);
        REQUIRE(snmpVersionFromString("1") == SnmpVersion::V1);
        REQUIRE(snmpVersionFromString("v2c") == SnmpVersion::V2c);
        REQUIRE(snmpVersionFromString("v3") == SnmpVersion::V3);
        REQUIRE(snmpVersionFromString("3") == SnmpVersion::V3);
        REQUIRE(snmpVersionFromString("invalid") == SnmpVersion::V2c);
    }
}

TEST_CASE("SNMP auth protocol string conversion", "[SnmpTypes]") {
    REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::None) == "None");
    REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::MD5) == "MD5");
    REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::SHA) == "SHA");
    REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::SHA256) == "SHA256");
}

TEST_CASE("SNMP priv protocol string conversion", "[SnmpTypes]") {
    REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::None) == "None");
    REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::DES) == "DES");
    REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::AES128) == "AES128");
    REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::AES256) == "AES256");
}

TEST_CASE("SNMP data type string conversion", "[SnmpTypes]") {
    REQUIRE(snmpDataTypeToString(SnmpDataType::Integer) == "INTEGER");
    REQUIRE(snmpDataTypeToString(SnmpDataType::OctetString) == "OCTET STRING");
    REQUIRE(snmpDataTypeToString(SnmpDataType::ObjectIdentifier) == "OBJECT IDENTIFIER");
    REQUIRE(snmpDataTypeToString(SnmpDataType::IpAddress) == "IpAddress");
    REQUIRE(snmpDataTypeToString(SnmpDataType::Counter32) == "Counter32");
    REQUIRE(snmpDataTypeToString(SnmpDataType::Gauge32) == "Gauge32");
    REQUIRE(snmpDataTypeToString(SnmpDataType::TimeTicks) == "TimeTicks");
    REQUIRE(snmpDataTypeToString(SnmpDataType::Counter64) == "Counter64");
    REQUIRE(snmpDataTypeToString(SnmpDataType::Null) == "Null");
    REQUIRE(snmpDataTypeToString(SnmpDataType::NoSuchObject) == "noSuchObject");
    REQUIRE(snmpDataTypeToString(SnmpDataType::NoSuchInstance) == "noSuchInstance");
    REQUIRE(snmpDataTypeToString(SnmpDataType::EndOfMibView) == "endOfMibView");
    REQUIRE(snmpDataTypeToString(SnmpDataType::Unknown) == "Unknown");
}

TEST_CASE("SnmpV2cCredentials default values", "[SnmpTypes][Credentials]") {
    SnmpV2cCredentials creds;
    REQUIRE(creds.community == "public");
}

TEST_CASE("SnmpV2cCredentials equality", "[SnmpTypes][Credentials]") {
    SnmpV2cCredentials creds1;
    creds1.community = "public";

    SnmpV2cCredentials creds2 = creds1;
    REQUIRE(creds1 == creds2);

    creds2.community = "private";
    REQUIRE_FALSE(creds1 == creds2);
}

TEST_CASE("SnmpV3Credentials default values", "[SnmpTypes][Credentials]") {
    SnmpV3Credentials creds;

    REQUIRE(creds.username.empty());
    REQUIRE(creds.securityLevel == SnmpSecurityLevel::NoAuthNoPriv);
    REQUIRE(creds.authProtocol == SnmpAuthProtocol::None);
    REQUIRE(creds.authPassword.empty());
    REQUIRE(creds.privProtocol == SnmpPrivProtocol::None);
    REQUIRE(creds.privPassword.empty());
    REQUIRE(creds.contextName.empty());
    REQUIRE(creds.contextEngineId.empty());
}

TEST_CASE("SnmpV3Credentials equality", "[SnmpTypes][Credentials]") {
    SnmpV3Credentials creds1;
    creds1.username = "admin";
    creds1.securityLevel = SnmpSecurityLevel::AuthPriv;
    creds1.authProtocol = SnmpAuthProtocol::SHA;
    creds1.authPassword = "authpass";
    creds1.privProtocol = SnmpPrivProtocol::AES128;
    creds1.privPassword = "privpass";

    SnmpV3Credentials creds2 = creds1;
    REQUIRE(creds1 == creds2);

    creds2.username = "different";
    REQUIRE_FALSE(creds1 == creds2);
}

TEST_CASE("SnmpVarBind default values", "[SnmpTypes][VarBind]") {
    SnmpVarBind vb;

    REQUIRE(vb.oid.empty());
    REQUIRE(vb.type == SnmpDataType::Unknown);
    REQUIRE(vb.value.empty());
    REQUIRE_FALSE(vb.intValue.has_value());
    REQUIRE_FALSE(vb.counterValue.has_value());
}

TEST_CASE("SnmpVarBind equality", "[SnmpTypes][VarBind]") {
    SnmpVarBind vb1;
    vb1.oid = "1.3.6.1.2.1.1.1.0";
    vb1.type = SnmpDataType::OctetString;
    vb1.value = "Linux server";

    SnmpVarBind vb2 = vb1;
    REQUIRE(vb1 == vb2);

    vb2.value = "Windows server";
    REQUIRE_FALSE(vb1 == vb2);
}

TEST_CASE("SnmpResult default values", "[SnmpTypes][Result]") {
    SnmpResult result;

    REQUIRE(result.id == 0);
    REQUIRE(result.hostId == 0);
    REQUIRE(result.version == SnmpVersion::V2c);
    REQUIRE(result.varbinds.empty());
    REQUIRE(result.responseTime == std::chrono::microseconds{0});
    REQUIRE(result.success == false);
    REQUIRE(result.errorMessage.empty());
    REQUIRE(result.errorStatus == 0);
    REQUIRE(result.errorIndex == 0);
}

TEST_CASE("SnmpResult responseTimeMs", "[SnmpTypes][Result]") {
    SnmpResult result;

    SECTION("Zero response time") {
        result.responseTime = std::chrono::microseconds{0};
        REQUIRE(result.responseTimeMs() == 0.0);
    }

    SECTION("One millisecond") {
        result.responseTime = std::chrono::microseconds{1000};
        REQUIRE(result.responseTimeMs() == 1.0);
    }

    SECTION("Fractional milliseconds") {
        result.responseTime = std::chrono::microseconds{1500};
        REQUIRE(result.responseTimeMs() == 1.5);
    }

    SECTION("Large response time") {
        result.responseTime = std::chrono::microseconds{5000000};
        REQUIRE(result.responseTimeMs() == 5000.0);
    }
}

TEST_CASE("SnmpResult getVarBind", "[SnmpTypes][Result]") {
    SnmpResult result;

    SnmpVarBind vb1;
    vb1.oid = "1.3.6.1.2.1.1.1.0";
    vb1.type = SnmpDataType::OctetString;
    vb1.value = "System description";

    SnmpVarBind vb2;
    vb2.oid = "1.3.6.1.2.1.1.3.0";
    vb2.type = SnmpDataType::TimeTicks;
    vb2.value = "123456789";

    result.varbinds = {vb1, vb2};

    SECTION("Find existing OID") {
        auto found = result.getVarBind("1.3.6.1.2.1.1.1.0");
        REQUIRE(found.has_value());
        REQUIRE(found->value == "System description");
    }

    SECTION("Find another existing OID") {
        auto found = result.getVarBind("1.3.6.1.2.1.1.3.0");
        REQUIRE(found.has_value());
        REQUIRE(found->type == SnmpDataType::TimeTicks);
    }

    SECTION("Non-existing OID") {
        auto found = result.getVarBind("1.3.6.1.2.1.1.5.0");
        REQUIRE_FALSE(found.has_value());
    }

    SECTION("Empty varbinds") {
        result.varbinds.clear();
        auto found = result.getVarBind("1.3.6.1.2.1.1.1.0");
        REQUIRE_FALSE(found.has_value());
    }
}

TEST_CASE("SnmpResult equality", "[SnmpTypes][Result]") {
    SnmpResult result1;
    result1.id = 1;
    result1.hostId = 10;
    result1.version = SnmpVersion::V2c;
    result1.success = true;
    result1.timestamp = std::chrono::system_clock::now();

    SnmpResult result2 = result1;
    REQUIRE(result1 == result2);

    result2.success = false;
    REQUIRE_FALSE(result1 == result2);
}

TEST_CASE("SnmpDeviceConfig default values", "[SnmpTypes][DeviceConfig]") {
    SnmpDeviceConfig config;

    REQUIRE(config.id == 0);
    REQUIRE(config.hostId == 0);
    REQUIRE(config.version == SnmpVersion::V2c);
    REQUIRE(config.port == 161);
    REQUIRE(config.timeoutMs == 5000);
    REQUIRE(config.retries == 1);
    REQUIRE(config.pollIntervalSeconds == 60);
    REQUIRE(config.oids.empty());
    REQUIRE(config.enabled == true);
    REQUIRE_FALSE(config.lastPolled.has_value());
}

TEST_CASE("SnmpDeviceConfig equality", "[SnmpTypes][DeviceConfig]") {
    SnmpDeviceConfig config1;
    config1.id = 1;
    config1.hostId = 10;
    config1.port = 161;
    config1.enabled = true;

    SnmpDeviceConfig config2 = config1;
    REQUIRE(config1 == config2);

    config2.port = 162;
    REQUIRE_FALSE(config1 == config2);
}

TEST_CASE("SnmpStatistics default values", "[SnmpTypes][Statistics]") {
    SnmpStatistics stats;

    REQUIRE(stats.hostId == 0);
    REQUIRE(stats.totalPolls == 0);
    REQUIRE(stats.successfulPolls == 0);
    REQUIRE(stats.minResponseTime == std::chrono::microseconds{0});
    REQUIRE(stats.maxResponseTime == std::chrono::microseconds{0});
    REQUIRE(stats.avgResponseTime == std::chrono::microseconds{0});
    REQUIRE(stats.successRate == 0.0);
    REQUIRE(stats.lastValues.empty());
}

TEST_CASE("SnmpStatistics calculateSuccessRate", "[SnmpTypes][Statistics]") {
    SnmpStatistics stats;

    SECTION("Zero polls") {
        stats.totalPolls = 0;
        stats.successfulPolls = 0;
        REQUIRE(stats.calculateSuccessRate() == 0.0);
    }

    SECTION("100% success rate") {
        stats.totalPolls = 100;
        stats.successfulPolls = 100;
        REQUIRE(stats.calculateSuccessRate() == 100.0);
    }

    SECTION("50% success rate") {
        stats.totalPolls = 100;
        stats.successfulPolls = 50;
        REQUIRE(stats.calculateSuccessRate() == 50.0);
    }

    SECTION("0% success rate") {
        stats.totalPolls = 100;
        stats.successfulPolls = 0;
        REQUIRE(stats.calculateSuccessRate() == 0.0);
    }

    SECTION("Partial success rate") {
        stats.totalPolls = 10;
        stats.successfulPolls = 7;
        REQUIRE(stats.calculateSuccessRate() == 70.0);
    }
}

TEST_CASE("SnmpOids constants", "[SnmpTypes][OIDs]") {
    REQUIRE(std::string(SnmpOids::SYS_DESCR) == "1.3.6.1.2.1.1.1.0");
    REQUIRE(std::string(SnmpOids::SYS_OBJECT_ID) == "1.3.6.1.2.1.1.2.0");
    REQUIRE(std::string(SnmpOids::SYS_UPTIME) == "1.3.6.1.2.1.1.3.0");
    REQUIRE(std::string(SnmpOids::SYS_CONTACT) == "1.3.6.1.2.1.1.4.0");
    REQUIRE(std::string(SnmpOids::SYS_NAME) == "1.3.6.1.2.1.1.5.0");
    REQUIRE(std::string(SnmpOids::SYS_LOCATION) == "1.3.6.1.2.1.1.6.0");

    REQUIRE(std::string(SnmpOids::IF_NUMBER) == "1.3.6.1.2.1.2.1.0");
    REQUIRE(std::string(SnmpOids::IF_TABLE) == "1.3.6.1.2.1.2.2");

    REQUIRE(std::string(SnmpOids::HR_SYSTEM_UPTIME) == "1.3.6.1.2.1.25.1.1.0");
}

TEST_CASE("SnmpCredentials variant", "[SnmpTypes][Credentials]") {
    SECTION("V2c credentials") {
        SnmpCredentials creds = SnmpV2cCredentials{"private"};
        REQUIRE(std::holds_alternative<SnmpV2cCredentials>(creds));
        auto& v2c = std::get<SnmpV2cCredentials>(creds);
        REQUIRE(v2c.community == "private");
    }

    SECTION("V3 credentials") {
        SnmpV3Credentials v3;
        v3.username = "admin";
        v3.securityLevel = SnmpSecurityLevel::AuthPriv;
        SnmpCredentials creds = v3;
        REQUIRE(std::holds_alternative<SnmpV3Credentials>(creds));
        auto& stored = std::get<SnmpV3Credentials>(creds);
        REQUIRE(stored.username == "admin");
    }
}
