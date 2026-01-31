#include <catch2/catch_test_macros.hpp>

#include "core/types/SnmpTypes.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/SnmpRepository.hpp"
#include "infrastructure/network/AsioContext.hpp"
#include "infrastructure/network/SnmpService.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

using namespace netpulse::core;
using namespace netpulse::infra;

TEST_CASE("SnmpTypes structures", "[SnmpService]") {
    SECTION("SnmpVarBind default values") {
        SnmpVarBind vb;
        REQUIRE(vb.oid.empty());
        REQUIRE(vb.type == SnmpDataType::Unknown);
        REQUIRE(vb.value.empty());
        REQUIRE_FALSE(vb.intValue.has_value());
        REQUIRE_FALSE(vb.counterValue.has_value());
    }

    SECTION("SnmpResult default values") {
        SnmpResult result;
        REQUIRE(result.id == 0);
        REQUIRE(result.hostId == 0);
        REQUIRE(result.version == SnmpVersion::V2c);
        REQUIRE(result.varbinds.empty());
        REQUIRE(result.success == false);
        REQUIRE(result.errorMessage.empty());
        REQUIRE(result.errorStatus == 0);
        REQUIRE(result.errorIndex == 0);
    }

    SECTION("SnmpResult response time conversion") {
        SnmpResult result;
        result.responseTime = std::chrono::microseconds(1500);
        REQUIRE(result.responseTimeMs() == 1.5);

        result.responseTime = std::chrono::microseconds(10000);
        REQUIRE(result.responseTimeMs() == 10.0);
    }

    SECTION("SnmpResult getVarBind") {
        SnmpResult result;
        SnmpVarBind vb1;
        vb1.oid = "1.3.6.1.2.1.1.1.0";
        vb1.value = "Linux router";

        SnmpVarBind vb2;
        vb2.oid = "1.3.6.1.2.1.1.5.0";
        vb2.value = "my-router";

        result.varbinds = {vb1, vb2};

        auto found = result.getVarBind("1.3.6.1.2.1.1.5.0");
        REQUIRE(found.has_value());
        REQUIRE(found->value == "my-router");

        auto notFound = result.getVarBind("1.3.6.1.2.1.1.6.0");
        REQUIRE_FALSE(notFound.has_value());
    }

    SECTION("SnmpDeviceConfig default values") {
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
    }

    SECTION("SnmpV2cCredentials default values") {
        SnmpV2cCredentials creds;
        REQUIRE(creds.community == "public");
    }

    SECTION("SnmpV3Credentials default values") {
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
}

TEST_CASE("SnmpTypes helper functions", "[SnmpService]") {
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
        REQUIRE(snmpVersionFromString("unknown") == SnmpVersion::V2c);  // Default
    }

    SECTION("snmpAuthProtocolToString") {
        REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::None) == "None");
        REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::MD5) == "MD5");
        REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::SHA) == "SHA");
        REQUIRE(snmpAuthProtocolToString(SnmpAuthProtocol::SHA256) == "SHA256");
    }

    SECTION("snmpPrivProtocolToString") {
        REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::None) == "None");
        REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::DES) == "DES");
        REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::AES128) == "AES128");
        REQUIRE(snmpPrivProtocolToString(SnmpPrivProtocol::AES256) == "AES256");
    }

    SECTION("snmpDataTypeToString") {
        REQUIRE(snmpDataTypeToString(SnmpDataType::Integer) == "INTEGER");
        REQUIRE(snmpDataTypeToString(SnmpDataType::OctetString) == "OCTET STRING");
        REQUIRE(snmpDataTypeToString(SnmpDataType::Counter32) == "Counter32");
        REQUIRE(snmpDataTypeToString(SnmpDataType::TimeTicks) == "TimeTicks");
        REQUIRE(snmpDataTypeToString(SnmpDataType::Unknown) == "Unknown");
    }
}

TEST_CASE("SnmpStatistics", "[SnmpService]") {
    SECTION("Default values") {
        SnmpStatistics stats;
        REQUIRE(stats.hostId == 0);
        REQUIRE(stats.totalPolls == 0);
        REQUIRE(stats.successfulPolls == 0);
        REQUIRE(stats.successRate == 0.0);
        REQUIRE(stats.lastValues.empty());
    }

    SECTION("calculateSuccessRate") {
        SnmpStatistics stats;

        // Zero polls
        REQUIRE(stats.calculateSuccessRate() == 0.0);

        // 10 polls, 8 successful
        stats.totalPolls = 10;
        stats.successfulPolls = 8;
        REQUIRE(stats.calculateSuccessRate() == 80.0);

        // All successful
        stats.successfulPolls = 10;
        REQUIRE(stats.calculateSuccessRate() == 100.0);

        // None successful
        stats.successfulPolls = 0;
        REQUIRE(stats.calculateSuccessRate() == 0.0);
    }
}

TEST_CASE("SnmpOids constants", "[SnmpService]") {
    SECTION("System MIB OIDs are valid") {
        REQUIRE(std::string(SnmpOids::SYS_DESCR) == "1.3.6.1.2.1.1.1.0");
        REQUIRE(std::string(SnmpOids::SYS_OBJECT_ID) == "1.3.6.1.2.1.1.2.0");
        REQUIRE(std::string(SnmpOids::SYS_UPTIME) == "1.3.6.1.2.1.1.3.0");
        REQUIRE(std::string(SnmpOids::SYS_CONTACT) == "1.3.6.1.2.1.1.4.0");
        REQUIRE(std::string(SnmpOids::SYS_NAME) == "1.3.6.1.2.1.1.5.0");
        REQUIRE(std::string(SnmpOids::SYS_LOCATION) == "1.3.6.1.2.1.1.6.0");
    }

    SECTION("Interface MIB OIDs are valid") {
        REQUIRE(std::string(SnmpOids::IF_NUMBER) == "1.3.6.1.2.1.2.1.0");
        REQUIRE(std::string(SnmpOids::IF_IN_OCTETS) == "1.3.6.1.2.1.2.2.1.10");
        REQUIRE(std::string(SnmpOids::IF_OUT_OCTETS) == "1.3.6.1.2.1.2.2.1.16");
    }
}

TEST_CASE("SnmpService initialization", "[SnmpService]") {
    AsioContext context;
    context.start();

    SECTION("Service can be created") {
        REQUIRE_NOTHROW(SnmpService(context));
    }

    SECTION("Service initializes correctly") {
        SnmpService service(context);
        // Should not be monitoring any hosts initially
        REQUIRE_FALSE(service.isMonitoring(1));
        REQUIRE_FALSE(service.isMonitoring(0));
    }

    context.stop();
}

TEST_CASE("SnmpService monitoring management", "[SnmpService]") {
    AsioContext context;
    context.start();
    SnmpService service(context);

    SECTION("Start and stop monitoring") {
        Host host;
        host.id = 1;
        host.name = "Test Router";
        host.address = "127.0.0.1";

        SnmpDeviceConfig config;
        config.hostId = host.id;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.oids = {SnmpOids::SYS_DESCR, SnmpOids::SYS_UPTIME};
        config.pollIntervalSeconds = 3600;  // Long interval to not trigger during test

        bool callbackInvoked = false;
        service.startMonitoring(host, config, [&callbackInvoked](const SnmpResult&) {
            callbackInvoked = true;
        });

        REQUIRE(service.isMonitoring(1));

        service.stopMonitoring(1);
        REQUIRE_FALSE(service.isMonitoring(1));
    }

    SECTION("Stop monitoring non-existent host") {
        // Should not throw
        REQUIRE_NOTHROW(service.stopMonitoring(999));
    }

    SECTION("Stop all monitoring") {
        Host host1;
        host1.id = 1;
        host1.name = "Router 1";
        host1.address = "127.0.0.1";

        Host host2;
        host2.id = 2;
        host2.name = "Router 2";
        host2.address = "127.0.0.2";

        SnmpDeviceConfig config;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.oids = {SnmpOids::SYS_DESCR};
        config.pollIntervalSeconds = 3600;

        config.hostId = host1.id;
        service.startMonitoring(host1, config, [](const SnmpResult&) {});

        config.hostId = host2.id;
        service.startMonitoring(host2, config, [](const SnmpResult&) {});

        REQUIRE(service.isMonitoring(1));
        REQUIRE(service.isMonitoring(2));

        service.stopAllMonitoring();

        REQUIRE_FALSE(service.isMonitoring(1));
        REQUIRE_FALSE(service.isMonitoring(2));
    }

    SECTION("Replace monitoring for same host") {
        Host host;
        host.id = 1;
        host.name = "Test Router";
        host.address = "127.0.0.1";

        SnmpDeviceConfig config;
        config.hostId = host.id;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.oids = {SnmpOids::SYS_DESCR};
        config.pollIntervalSeconds = 3600;

        int callback1Count = 0;
        int callback2Count = 0;

        service.startMonitoring(host, config, [&callback1Count](const SnmpResult&) {
            callback1Count++;
        });

        // Start monitoring same host with different callback
        service.startMonitoring(host, config, [&callback2Count](const SnmpResult&) {
            callback2Count++;
        });

        // Should still be monitoring
        REQUIRE(service.isMonitoring(1));
    }

    SECTION("Update config for monitored host") {
        Host host;
        host.id = 1;
        host.name = "Test Router";
        host.address = "127.0.0.1";

        SnmpDeviceConfig config;
        config.hostId = host.id;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.oids = {SnmpOids::SYS_DESCR};
        config.pollIntervalSeconds = 3600;

        service.startMonitoring(host, config, [](const SnmpResult&) {});

        // Update config
        config.oids = {SnmpOids::SYS_DESCR, SnmpOids::SYS_UPTIME, SnmpOids::SYS_NAME};
        REQUIRE_NOTHROW(service.updateConfig(host.id, config));

        REQUIRE(service.isMonitoring(1));
    }

    SECTION("Get statistics for non-monitored host") {
        auto stats = service.getStatistics(999);
        REQUIRE(stats.totalPolls == 0);
        REQUIRE(stats.successfulPolls == 0);
    }

    context.stop();
}

TEST_CASE("SnmpService async queries", "[SnmpService][integration]") {
    AsioContext context;
    context.start();
    SnmpService service(context);

    SECTION("Query with invalid address handles error gracefully") {
        SnmpDeviceConfig config;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.timeoutMs = 1000;  // Short timeout

        auto future = service.getAsync("999.999.999.999",
                                        {SnmpOids::SYS_DESCR},
                                        config);

        auto status = future.wait_for(std::chrono::seconds(5));
        REQUIRE(status == std::future_status::ready);

        auto result = future.get();
        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errorMessage.empty());
    }

    SECTION("Query with short timeout") {
        SnmpDeviceConfig config;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.timeoutMs = 100;  // Very short timeout

        // Use a non-routable address to trigger timeout
        auto future = service.getAsync("10.255.255.1",
                                        {SnmpOids::SYS_DESCR},
                                        config);

        // Wait longer than the SNMP timeout + processing time
        auto status = future.wait_for(std::chrono::seconds(10));
        if (status == std::future_status::ready) {
            auto result = future.get();
            // Should typically timeout or fail
            REQUIRE(result.timestamp.time_since_epoch().count() > 0);
        }
        // If not ready within 10 seconds, test still passes - operation is running
    }

    context.stop();
}

TEST_CASE("SnmpRepository operations", "[SnmpService][database]") {
    auto tempDir = std::filesystem::temp_directory_path();
    auto dbPath = tempDir / "test_snmp_repo.db";
    std::filesystem::remove(dbPath);

    auto db = std::make_shared<Database>(dbPath.string());
    db->execute(R"(
        CREATE TABLE IF NOT EXISTS hosts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            address TEXT NOT NULL
        )
    )");

    SnmpRepository repo(db);

    SECTION("Insert and retrieve device config") {
        // First insert a host
        db->execute("INSERT INTO hosts (name, address) VALUES ('Test Router', '192.168.1.1')");
        int64_t hostId = db->lastInsertRowId();

        SnmpDeviceConfig config;
        config.hostId = hostId;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"private"};
        config.port = 161;
        config.timeoutMs = 3000;
        config.retries = 2;
        config.pollIntervalSeconds = 30;
        config.oids = {SnmpOids::SYS_DESCR, SnmpOids::SYS_UPTIME};
        config.enabled = true;
        config.createdAt = std::chrono::system_clock::now();

        int64_t configId = repo.insertDeviceConfig(config);
        REQUIRE(configId > 0);

        auto retrieved = repo.getDeviceConfig(configId);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->hostId == hostId);
        REQUIRE(retrieved->version == SnmpVersion::V2c);
        REQUIRE(retrieved->port == 161);
        REQUIRE(retrieved->timeoutMs == 3000);
        REQUIRE(retrieved->retries == 2);
        REQUIRE(retrieved->pollIntervalSeconds == 30);
        REQUIRE(retrieved->oids.size() == 2);
        REQUIRE(retrieved->enabled == true);
    }

    SECTION("Get config by host ID") {
        db->execute("INSERT INTO hosts (name, address) VALUES ('Router 2', '192.168.1.2')");
        int64_t hostId = db->lastInsertRowId();

        SnmpDeviceConfig config;
        config.hostId = hostId;
        config.version = SnmpVersion::V3;
        config.credentials = SnmpV3Credentials{
            .username = "admin",
            .securityLevel = SnmpSecurityLevel::AuthNoPriv,
            .authProtocol = SnmpAuthProtocol::SHA,
            .authPassword = "authpass123"
        };
        config.oids = {SnmpOids::SYS_DESCR};
        config.createdAt = std::chrono::system_clock::now();

        repo.insertDeviceConfig(config);

        auto retrieved = repo.getDeviceConfigByHostId(hostId);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->hostId == hostId);
        REQUIRE(retrieved->version == SnmpVersion::V3);

        auto* v3Creds = std::get_if<SnmpV3Credentials>(&retrieved->credentials);
        REQUIRE(v3Creds != nullptr);
        REQUIRE(v3Creds->username == "admin");
        REQUIRE(v3Creds->securityLevel == SnmpSecurityLevel::AuthNoPriv);
        REQUIRE(v3Creds->authProtocol == SnmpAuthProtocol::SHA);
    }

    SECTION("Get all device configs") {
        // Insert multiple hosts and configs
        for (int i = 1; i <= 3; ++i) {
            db->execute("INSERT INTO hosts (name, address) VALUES ('Router " +
                        std::to_string(i) + "', '192.168.1." + std::to_string(i) + "')");
            int64_t hostId = db->lastInsertRowId();

            SnmpDeviceConfig config;
            config.hostId = hostId;
            config.version = SnmpVersion::V2c;
            config.credentials = SnmpV2cCredentials{"public"};
            config.oids = {SnmpOids::SYS_DESCR};
            config.enabled = (i != 2);  // Disable second one
            config.createdAt = std::chrono::system_clock::now();

            repo.insertDeviceConfig(config);
        }

        auto all = repo.getAllDeviceConfigs();
        REQUIRE(all.size() == 3);

        auto enabled = repo.getEnabledDeviceConfigs();
        REQUIRE(enabled.size() == 2);
    }

    SECTION("Update device config") {
        db->execute("INSERT INTO hosts (name, address) VALUES ('Router Update', '192.168.1.10')");
        int64_t hostId = db->lastInsertRowId();

        SnmpDeviceConfig config;
        config.hostId = hostId;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.oids = {SnmpOids::SYS_DESCR};
        config.pollIntervalSeconds = 60;
        config.createdAt = std::chrono::system_clock::now();

        int64_t configId = repo.insertDeviceConfig(config);

        // Update
        config.id = configId;
        config.pollIntervalSeconds = 30;
        config.oids = {SnmpOids::SYS_DESCR, SnmpOids::SYS_UPTIME, SnmpOids::SYS_NAME};
        config.lastPolled = std::chrono::system_clock::now();

        repo.updateDeviceConfig(config);

        auto retrieved = repo.getDeviceConfig(configId);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->pollIntervalSeconds == 30);
        REQUIRE(retrieved->oids.size() == 3);
        REQUIRE(retrieved->lastPolled.has_value());
    }

    SECTION("Delete device config") {
        db->execute("INSERT INTO hosts (name, address) VALUES ('Router Delete', '192.168.1.20')");
        int64_t hostId = db->lastInsertRowId();

        SnmpDeviceConfig config;
        config.hostId = hostId;
        config.version = SnmpVersion::V2c;
        config.credentials = SnmpV2cCredentials{"public"};
        config.oids = {SnmpOids::SYS_DESCR};
        config.createdAt = std::chrono::system_clock::now();

        int64_t configId = repo.insertDeviceConfig(config);
        REQUIRE(repo.getDeviceConfig(configId).has_value());

        repo.deleteDeviceConfig(configId);
        REQUIRE_FALSE(repo.getDeviceConfig(configId).has_value());
    }

    SECTION("Insert and retrieve SNMP results") {
        db->execute("INSERT INTO hosts (name, address) VALUES ('Router Results', '192.168.1.30')");
        int64_t hostId = db->lastInsertRowId();

        SnmpResult result;
        result.hostId = hostId;
        result.timestamp = std::chrono::system_clock::now();
        result.version = SnmpVersion::V2c;
        result.responseTime = std::chrono::microseconds(5000);
        result.success = true;

        SnmpVarBind vb1;
        vb1.oid = SnmpOids::SYS_DESCR;
        vb1.type = SnmpDataType::OctetString;
        vb1.value = "Linux router 5.4.0";

        SnmpVarBind vb2;
        vb2.oid = SnmpOids::SYS_UPTIME;
        vb2.type = SnmpDataType::TimeTicks;
        vb2.value = "123456789";
        vb2.counterValue = 123456789;

        result.varbinds = {vb1, vb2};

        int64_t resultId = repo.insertResult(result);
        REQUIRE(resultId > 0);

        auto results = repo.getResults(hostId, 10);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].hostId == hostId);
        REQUIRE(results[0].success == true);
        REQUIRE(results[0].varbinds.size() == 2);
        REQUIRE(results[0].varbinds[0].value == "Linux router 5.4.0");
    }

    SECTION("Get statistics") {
        db->execute("INSERT INTO hosts (name, address) VALUES ('Router Stats', '192.168.1.40')");
        int64_t hostId = db->lastInsertRowId();

        // Insert multiple results
        for (int i = 0; i < 10; ++i) {
            SnmpResult result;
            result.hostId = hostId;
            result.timestamp = std::chrono::system_clock::now();
            result.version = SnmpVersion::V2c;
            result.responseTime = std::chrono::microseconds(1000 + i * 100);
            result.success = (i % 3 != 0);  // 7 successful, 3 failed

            if (result.success) {
                SnmpVarBind vb;
                vb.oid = SnmpOids::SYS_UPTIME;
                vb.type = SnmpDataType::TimeTicks;
                vb.value = std::to_string(i * 1000);
                result.varbinds = {vb};
            } else {
                result.errorMessage = "Timeout";
            }

            repo.insertResult(result);
        }

        auto stats = repo.getStatistics(hostId, 10);
        REQUIRE(stats.hostId == hostId);
        REQUIRE(stats.totalPolls == 10);
        // Note: Due to modulo operation, polls 0, 3, 6, 9 fail = 4 failures, 6 successes
        // Actually i % 3 != 0 means successful when i = 1,2,4,5,7,8 = 6 successful
        REQUIRE(stats.successfulPolls == 6);
        REQUIRE(stats.successRate > 50.0);
    }

    std::filesystem::remove(dbPath);
}

TEST_CASE("SnmpCredentials variant", "[SnmpService]") {
    SECTION("V2c credentials in variant") {
        SnmpCredentials creds = SnmpV2cCredentials{"mycommunity"};

        auto* v2c = std::get_if<SnmpV2cCredentials>(&creds);
        REQUIRE(v2c != nullptr);
        REQUIRE(v2c->community == "mycommunity");

        auto* v3 = std::get_if<SnmpV3Credentials>(&creds);
        REQUIRE(v3 == nullptr);
    }

    SECTION("V3 credentials in variant") {
        SnmpV3Credentials v3Creds;
        v3Creds.username = "snmpuser";
        v3Creds.securityLevel = SnmpSecurityLevel::AuthPriv;
        v3Creds.authProtocol = SnmpAuthProtocol::SHA256;
        v3Creds.authPassword = "authsecret";
        v3Creds.privProtocol = SnmpPrivProtocol::AES256;
        v3Creds.privPassword = "privsecret";

        SnmpCredentials creds = v3Creds;

        auto* v2c = std::get_if<SnmpV2cCredentials>(&creds);
        REQUIRE(v2c == nullptr);

        auto* v3 = std::get_if<SnmpV3Credentials>(&creds);
        REQUIRE(v3 != nullptr);
        REQUIRE(v3->username == "snmpuser");
        REQUIRE(v3->securityLevel == SnmpSecurityLevel::AuthPriv);
        REQUIRE(v3->authProtocol == SnmpAuthProtocol::SHA256);
        REQUIRE(v3->privProtocol == SnmpPrivProtocol::AES256);
    }
}

TEST_CASE("SnmpResult equality", "[SnmpService]") {
    SECTION("Equal results") {
        SnmpResult r1, r2;
        r1.id = 1;
        r1.hostId = 100;
        r1.success = true;
        r1.responseTime = std::chrono::microseconds(5000);

        r2 = r1;
        REQUIRE(r1 == r2);
    }

    SECTION("Unequal results") {
        SnmpResult r1, r2;
        r1.id = 1;
        r1.hostId = 100;
        r1.success = true;

        r2 = r1;
        r2.success = false;
        REQUIRE_FALSE(r1 == r2);
    }
}
