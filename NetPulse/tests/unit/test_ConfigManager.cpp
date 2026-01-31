#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "infrastructure/config/ConfigManager.hpp"

#include <filesystem>
#include <fstream>

using namespace netpulse::infra;
using namespace netpulse::core;

namespace {

class TestConfigDir {
public:
    TestConfigDir()
        : configDir_(std::filesystem::temp_directory_path() / "netpulse_config_test") {
        cleanup();
        std::filesystem::create_directories(configDir_);
    }

    ~TestConfigDir() { cleanup(); }

    std::filesystem::path path() const { return configDir_; }

private:
    void cleanup() {
        if (std::filesystem::exists(configDir_)) {
            std::filesystem::remove_all(configDir_);
        }
    }

    std::filesystem::path configDir_;
};

} // namespace

TEST_CASE("ConfigManager constructor", "[ConfigManager]") {
    SECTION("Creates config directory if it does not exist") {
        auto tempPath = std::filesystem::temp_directory_path() / "netpulse_config_new_test";
        std::filesystem::remove_all(tempPath);

        REQUIRE_FALSE(std::filesystem::exists(tempPath));

        ConfigManager manager(tempPath);

        REQUIRE(std::filesystem::exists(tempPath));
        REQUIRE(std::filesystem::is_directory(tempPath));

        std::filesystem::remove_all(tempPath);
    }

    SECTION("Works with existing directory") {
        TestConfigDir testDir;

        REQUIRE_NOTHROW(ConfigManager(testDir.path()));
    }

    SECTION("Sets correct config path") {
        TestConfigDir testDir;
        ConfigManager manager(testDir.path());

        REQUIRE(manager.configPath() == testDir.path() / "config.json");
    }
}

TEST_CASE("ConfigManager path methods", "[ConfigManager]") {
    TestConfigDir testDir;
    ConfigManager manager(testDir.path());

    SECTION("configPath returns path to config.json") {
        REQUIRE(manager.configPath() == testDir.path() / "config.json");
    }

    SECTION("databasePath returns path to netpulse.db") {
        REQUIRE(manager.databasePath() == testDir.path() / "netpulse.db");
    }

    SECTION("pluginDir returns path to plugins directory") {
        REQUIRE(manager.pluginDir() == testDir.path() / "plugins");
    }

    SECTION("pluginStatePath returns path to plugin_state.json") {
        REQUIRE(manager.pluginStatePath() == testDir.path() / "plugin_state.json");
    }

    SECTION("configDir returns directory as string") {
        REQUIRE(manager.configDir() == testDir.path().string());
    }
}

TEST_CASE("ConfigManager load operations", "[ConfigManager]") {
    TestConfigDir testDir;

    SECTION("load creates config file with defaults when file does not exist") {
        ConfigManager manager(testDir.path());

        REQUIRE_FALSE(std::filesystem::exists(manager.configPath()));

        bool result = manager.load();

        REQUIRE(result);
        REQUIRE(std::filesystem::exists(manager.configPath()));
    }

    SECTION("load returns defaults when config file does not exist") {
        ConfigManager manager(testDir.path());
        manager.load();

        const auto& config = manager.config();
        REQUIRE(config.theme == "dark");
        REQUIRE(config.startMinimized == false);
        REQUIRE(config.minimizeToTray == true);
        REQUIRE(config.startOnLogin == false);
        REQUIRE(config.defaultPingIntervalSeconds == 30);
        REQUIRE(config.defaultWarningThresholdMs == 100);
        REQUIRE(config.defaultCriticalThresholdMs == 500);
        REQUIRE(config.desktopNotifications == true);
        REQUIRE(config.soundAlerts == false);
        REQUIRE(config.dataRetentionDays == 30);
        REQUIRE(config.autoCleanup == true);
        REQUIRE(config.portScanConcurrency == 100);
        REQUIRE(config.portScanTimeoutMs == 1000);
        REQUIRE(config.webhooksEnabled == true);
        REQUIRE(config.webhookTimeoutMs == 5000);
        REQUIRE(config.webhookMaxRetries == 3);
        REQUIRE(config.restApiEnabled == false);
        REQUIRE(config.restApiPort == 8080);
        REQUIRE(config.pluginsEnabled == true);
        REQUIRE(config.plugins.empty());
    }

    SECTION("load reads existing config file") {
        std::filesystem::create_directories(testDir.path());
        auto configPath = testDir.path() / "config.json";

        nlohmann::json j;
        j["general"]["theme"] = "light";
        j["general"]["start_minimized"] = true;
        j["general"]["minimize_to_tray"] = false;
        j["monitoring"]["default_ping_interval_seconds"] = 60;
        j["alerts"]["latency_warning_ms"] = 200;
        j["rest_api"]["enabled"] = true;
        j["rest_api"]["port"] = 9090;

        std::ofstream file(configPath);
        file << j.dump(2);
        file.close();

        ConfigManager manager(testDir.path());
        bool result = manager.load();

        REQUIRE(result);
        REQUIRE(manager.config().theme == "light");
        REQUIRE(manager.config().startMinimized == true);
        REQUIRE(manager.config().minimizeToTray == false);
        REQUIRE(manager.config().defaultPingIntervalSeconds == 60);
        REQUIRE(manager.config().alertThresholds.latencyWarningMs == 200);
        REQUIRE(manager.config().restApiEnabled == true);
        REQUIRE(manager.config().restApiPort == 9090);
    }

    SECTION("load handles partial config with defaults for missing fields") {
        auto configPath = testDir.path() / "config.json";

        nlohmann::json j;
        j["general"]["theme"] = "custom";

        std::ofstream file(configPath);
        file << j.dump(2);
        file.close();

        ConfigManager manager(testDir.path());
        manager.load();

        REQUIRE(manager.config().theme == "custom");
        REQUIRE(manager.config().defaultPingIntervalSeconds == 30);
        REQUIRE(manager.config().restApiPort == 8080);
    }

    SECTION("load returns false for invalid JSON") {
        auto configPath = testDir.path() / "config.json";

        std::ofstream file(configPath);
        file << "{ invalid json content }}}";
        file.close();

        ConfigManager manager(testDir.path());
        bool result = manager.load();

        REQUIRE_FALSE(result);
    }
}

TEST_CASE("ConfigManager save operations", "[ConfigManager]") {
    TestConfigDir testDir;

    SECTION("save creates config file") {
        ConfigManager manager(testDir.path());

        REQUIRE_FALSE(std::filesystem::exists(manager.configPath()));

        bool result = manager.save();

        REQUIRE(result);
        REQUIRE(std::filesystem::exists(manager.configPath()));
    }

    SECTION("save persists configuration changes") {
        ConfigManager manager(testDir.path());
        manager.config().theme = "light";
        manager.config().defaultPingIntervalSeconds = 120;
        manager.config().restApiEnabled = true;
        manager.config().restApiPort = 9999;

        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();

        REQUIRE(manager2.config().theme == "light");
        REQUIRE(manager2.config().defaultPingIntervalSeconds == 120);
        REQUIRE(manager2.config().restApiEnabled == true);
        REQUIRE(manager2.config().restApiPort == 9999);
    }

    SECTION("save preserves all configuration fields") {
        ConfigManager manager(testDir.path());

        auto& config = manager.config();
        config.theme = "custom";
        config.startMinimized = true;
        config.minimizeToTray = false;
        config.startOnLogin = true;
        config.defaultPingIntervalSeconds = 45;
        config.defaultWarningThresholdMs = 150;
        config.defaultCriticalThresholdMs = 750;
        config.alertThresholds.latencyWarningMs = 250;
        config.alertThresholds.latencyCriticalMs = 750;
        config.alertThresholds.packetLossWarningPercent = 10.0;
        config.alertThresholds.packetLossCriticalPercent = 30.0;
        config.alertThresholds.consecutiveFailuresForDown = 5;
        config.desktopNotifications = false;
        config.soundAlerts = true;
        config.dataRetentionDays = 60;
        config.autoCleanup = false;
        config.portScanConcurrency = 200;
        config.portScanTimeoutMs = 2000;
        config.windowX = 200;
        config.windowY = 150;
        config.windowWidth = 1400;
        config.windowHeight = 900;
        config.windowMaximized = true;
        config.webhooksEnabled = false;
        config.webhookTimeoutMs = 10000;
        config.webhookMaxRetries = 5;
        config.restApiEnabled = true;
        config.restApiPort = 8888;
        config.pluginsEnabled = false;

        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();

        const auto& loaded = manager2.config();
        REQUIRE(loaded.theme == "custom");
        REQUIRE(loaded.startMinimized == true);
        REQUIRE(loaded.minimizeToTray == false);
        REQUIRE(loaded.startOnLogin == true);
        REQUIRE(loaded.defaultPingIntervalSeconds == 45);
        REQUIRE(loaded.defaultWarningThresholdMs == 150);
        REQUIRE(loaded.defaultCriticalThresholdMs == 750);
        REQUIRE(loaded.alertThresholds.latencyWarningMs == 250);
        REQUIRE(loaded.alertThresholds.latencyCriticalMs == 750);
        REQUIRE_THAT(loaded.alertThresholds.packetLossWarningPercent,
                     Catch::Matchers::WithinRel(10.0, 0.001));
        REQUIRE_THAT(loaded.alertThresholds.packetLossCriticalPercent,
                     Catch::Matchers::WithinRel(30.0, 0.001));
        REQUIRE(loaded.alertThresholds.consecutiveFailuresForDown == 5);
        REQUIRE(loaded.desktopNotifications == false);
        REQUIRE(loaded.soundAlerts == true);
        REQUIRE(loaded.dataRetentionDays == 60);
        REQUIRE(loaded.autoCleanup == false);
        REQUIRE(loaded.portScanConcurrency == 200);
        REQUIRE(loaded.portScanTimeoutMs == 2000);
        REQUIRE(loaded.windowX == 200);
        REQUIRE(loaded.windowY == 150);
        REQUIRE(loaded.windowWidth == 1400);
        REQUIRE(loaded.windowHeight == 900);
        REQUIRE(loaded.windowMaximized == true);
        REQUIRE(loaded.webhooksEnabled == false);
        REQUIRE(loaded.webhookTimeoutMs == 10000);
        REQUIRE(loaded.webhookMaxRetries == 5);
        REQUIRE(loaded.restApiEnabled == true);
        REQUIRE(loaded.restApiPort == 8888);
        REQUIRE(loaded.pluginsEnabled == false);
    }

    SECTION("save persists plugin configurations") {
        ConfigManager manager(testDir.path());

        PluginConfig plugin1;
        plugin1.id = "test-plugin-1";
        plugin1.path = "/path/to/plugin1.so";
        plugin1.enabled = true;
        plugin1.settings = {{"setting1", "value1"}, {"setting2", 42}};

        PluginConfig plugin2;
        plugin2.id = "test-plugin-2";
        plugin2.path = "/path/to/plugin2.so";
        plugin2.enabled = false;
        plugin2.settings = {{"debug", true}};

        manager.config().plugins.push_back(plugin1);
        manager.config().plugins.push_back(plugin2);

        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();

        const auto& plugins = manager2.config().plugins;
        REQUIRE(plugins.size() == 2);
        REQUIRE(plugins[0].id == "test-plugin-1");
        REQUIRE(plugins[0].path == "/path/to/plugin1.so");
        REQUIRE(plugins[0].enabled == true);
        REQUIRE(plugins[0].settings["setting1"] == "value1");
        REQUIRE(plugins[0].settings["setting2"] == 42);
        REQUIRE(plugins[1].id == "test-plugin-2");
        REQUIRE(plugins[1].path == "/path/to/plugin2.so");
        REQUIRE(plugins[1].enabled == false);
        REQUIRE(plugins[1].settings["debug"] == true);
    }
}

TEST_CASE("ConfigManager secure storage", "[ConfigManager]") {
    TestConfigDir testDir;
    ConfigManager manager(testDir.path());

    SECTION("setSecureValue and getSecureValue roundtrip") {
        manager.setSecureValue("api_key", "secret123");

        auto retrieved = manager.getSecureValue("api_key");
        REQUIRE(retrieved.has_value());
        REQUIRE(*retrieved == "secret123");
    }

    SECTION("getSecureValue returns nullopt for non-existent key") {
        auto result = manager.getSecureValue("nonexistent_key");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("setSecureValue overwrites existing value") {
        manager.setSecureValue("password", "original");
        manager.setSecureValue("password", "updated");

        auto retrieved = manager.getSecureValue("password");
        REQUIRE(retrieved.has_value());
        REQUIRE(*retrieved == "updated");
    }

    SECTION("Multiple secure values can be stored") {
        manager.setSecureValue("key1", "value1");
        manager.setSecureValue("key2", "value2");
        manager.setSecureValue("key3", "value3");

        REQUIRE(manager.getSecureValue("key1").value() == "value1");
        REQUIRE(manager.getSecureValue("key2").value() == "value2");
        REQUIRE(manager.getSecureValue("key3").value() == "value3");
    }

    SECTION("Secure values persist after save/load") {
        manager.setSecureValue("persistent_secret", "mypassword");

        ConfigManager manager2(testDir.path());
        manager2.load();

        auto retrieved = manager2.getSecureValue("persistent_secret");
        REQUIRE(retrieved.has_value());
        REQUIRE(*retrieved == "mypassword");
    }

    SECTION("Secure values with special characters") {
        std::string specialValue = "p@$$w0rd!#$%^&*()_+-=[]{}|;':\",./<>?";
        manager.setSecureValue("special_key", specialValue);

        auto retrieved = manager.getSecureValue("special_key");
        REQUIRE(retrieved.has_value());
        REQUIRE(*retrieved == specialValue);
    }

    SECTION("Secure values with empty string") {
        manager.setSecureValue("empty_key", "");

        auto retrieved = manager.getSecureValue("empty_key");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->empty());
    }

    SECTION("Secure values with long string") {
        std::string longValue(1000, 'x');
        manager.setSecureValue("long_key", longValue);

        auto retrieved = manager.getSecureValue("long_key");
        REQUIRE(retrieved.has_value());
        REQUIRE(*retrieved == longValue);
    }
}

TEST_CASE("ConfigManager config access", "[ConfigManager]") {
    TestConfigDir testDir;
    ConfigManager manager(testDir.path());

    SECTION("config() returns mutable reference") {
        manager.config().theme = "modified";
        REQUIRE(manager.config().theme == "modified");
    }

    SECTION("const config() returns const reference") {
        const ConfigManager& constManager = manager;
        const AppConfig& config = constManager.config();
        REQUIRE(config.theme == "dark");
    }
}

TEST_CASE("ConfigManager edge cases", "[ConfigManager]") {
    TestConfigDir testDir;

    SECTION("Empty theme string is preserved") {
        ConfigManager manager(testDir.path());
        manager.config().theme = "";
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE(manager2.config().theme.empty());
    }

    SECTION("Zero values are preserved") {
        ConfigManager manager(testDir.path());
        manager.config().defaultPingIntervalSeconds = 0;
        manager.config().windowX = 0;
        manager.config().windowY = 0;
        manager.config().windowWidth = 0;
        manager.config().windowHeight = 0;
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE(manager2.config().defaultPingIntervalSeconds == 0);
        REQUIRE(manager2.config().windowX == 0);
        REQUIRE(manager2.config().windowY == 0);
        REQUIRE(manager2.config().windowWidth == 0);
        REQUIRE(manager2.config().windowHeight == 0);
    }

    SECTION("Large values are preserved") {
        ConfigManager manager(testDir.path());
        manager.config().dataRetentionDays = 365000;
        manager.config().portScanConcurrency = 100000;
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE(manager2.config().dataRetentionDays == 365000);
        REQUIRE(manager2.config().portScanConcurrency == 100000);
    }

    SECTION("Negative window coordinates are preserved") {
        ConfigManager manager(testDir.path());
        manager.config().windowX = -100;
        manager.config().windowY = -50;
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE(manager2.config().windowX == -100);
        REQUIRE(manager2.config().windowY == -50);
    }

    SECTION("Multiple save operations work correctly") {
        ConfigManager manager(testDir.path());

        manager.config().theme = "first";
        manager.save();

        manager.config().theme = "second";
        manager.save();

        manager.config().theme = "third";
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE(manager2.config().theme == "third");
    }

    SECTION("Empty plugins list is handled") {
        ConfigManager manager(testDir.path());
        manager.config().plugins.clear();
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE(manager2.config().plugins.empty());
    }

    SECTION("Plugin with empty settings is handled") {
        ConfigManager manager(testDir.path());

        PluginConfig plugin;
        plugin.id = "empty-settings-plugin";
        plugin.path = "/path/to/plugin.so";
        plugin.enabled = true;
        plugin.settings = nlohmann::json::object();

        manager.config().plugins.push_back(plugin);
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();

        REQUIRE(manager2.config().plugins.size() == 1);
        REQUIRE(manager2.config().plugins[0].settings.empty());
    }

    SECTION("Theme with special characters") {
        ConfigManager manager(testDir.path());
        manager.config().theme = "my-custom_theme.v2";
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE(manager2.config().theme == "my-custom_theme.v2");
    }

    SECTION("Alert thresholds with floating point precision") {
        ConfigManager manager(testDir.path());
        manager.config().alertThresholds.packetLossWarningPercent = 5.555;
        manager.config().alertThresholds.packetLossCriticalPercent = 25.789;
        manager.save();

        ConfigManager manager2(testDir.path());
        manager2.load();
        REQUIRE_THAT(manager2.config().alertThresholds.packetLossWarningPercent,
                     Catch::Matchers::WithinRel(5.555, 0.001));
        REQUIRE_THAT(manager2.config().alertThresholds.packetLossCriticalPercent,
                     Catch::Matchers::WithinRel(25.789, 0.001));
    }
}
