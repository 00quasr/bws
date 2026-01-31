#include <catch2/catch_test_macros.hpp>

#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IPluginContext.hpp"
#include "core/plugin/PluginHooks.hpp"
#include "infrastructure/plugin/PluginContext.hpp"
#include "infrastructure/plugin/PluginManager.hpp"

#include <filesystem>
#include <fstream>

using namespace netpulse::core;
using namespace netpulse::infra;

class MockPlugin : public IPlugin {
public:
    MockPlugin() {
        metadata_.id = "com.test.mock";
        metadata_.name = "Mock Plugin";
        metadata_.version = "1.0.0";
        metadata_.author = "Test";
        metadata_.description = "Mock plugin for testing";
        metadata_.type = PluginType::DataProcessor;
        metadata_.minAppVersion = "1.0.0";
    }

    const PluginMetadata& metadata() const override { return metadata_; }
    PluginState state() const override { return state_; }

    bool initialize(IPluginContext* context) override {
        if (!context) return false;
        context_ = context;
        state_ = PluginState::Initialized;
        initializeCalled_ = true;
        return true;
    }

    void shutdown() override {
        state_ = PluginState::Stopped;
        shutdownCalled_ = true;
    }

    void configure(const nlohmann::json& config) override {
        config_ = config;
        configureCalled_ = true;
    }

    nlohmann::json configuration() const override { return config_; }
    nlohmann::json defaultConfiguration() const override { return {{"default", true}}; }

    bool isHealthy() const override { return state_ == PluginState::Initialized; }
    std::string statusMessage() const override { return "OK"; }
    nlohmann::json diagnostics() const override { return {{"state", pluginStateToString(state_)}}; }

    void enable() override { enabled_ = true; }
    void disable() override { enabled_ = false; }
    bool isEnabled() const override { return enabled_; }

    bool initializeCalled_{false};
    bool shutdownCalled_{false};
    bool configureCalled_{false};

private:
    PluginMetadata metadata_;
    PluginState state_{PluginState::Loaded};
    IPluginContext* context_{nullptr};
    nlohmann::json config_;
    bool enabled_{true};
};

TEST_CASE("PluginMetadata serialization", "[Plugin]") {
    SECTION("To JSON") {
        PluginMetadata meta;
        meta.id = "com.test.plugin";
        meta.name = "Test Plugin";
        meta.version = "2.0.0";
        meta.author = "Test Author";
        meta.description = "Test description";
        meta.license = "MIT";
        meta.type = PluginType::NetworkMonitor;
        meta.minAppVersion = "1.5.0";
        meta.capabilities.push_back({"cap1", "1.0.0", "Capability 1"});
        meta.dependencies.push_back({"other.plugin", "1.0.0", true});

        auto j = meta.toJson();

        REQUIRE(j["id"] == "com.test.plugin");
        REQUIRE(j["name"] == "Test Plugin");
        REQUIRE(j["version"] == "2.0.0");
        REQUIRE(j["author"] == "Test Author");
        REQUIRE(j["description"] == "Test description");
        REQUIRE(j["license"] == "MIT");
        REQUIRE(j["type"] == 0);
        REQUIRE(j["minAppVersion"] == "1.5.0");
        REQUIRE(j["capabilities"].size() == 1);
        REQUIRE(j["dependencies"].size() == 1);
    }

    SECTION("From JSON") {
        nlohmann::json j = {
            {"id", "com.test.loaded"},
            {"name", "Loaded Plugin"},
            {"version", "3.0.0"},
            {"author", "Author"},
            {"description", "Description"},
            {"license", "Apache-2.0"},
            {"type", 1},
            {"minAppVersion", "2.0.0"},
            {"capabilities", {{{"name", "cap"}, {"version", "1.0"}, {"description", "desc"}}}},
            {"dependencies", {{{"pluginName", "dep"}, {"minVersion", "1.0"}, {"required", false}}}}
        };

        auto meta = PluginMetadata::fromJson(j);

        REQUIRE(meta.id == "com.test.loaded");
        REQUIRE(meta.name == "Loaded Plugin");
        REQUIRE(meta.version == "3.0.0");
        REQUIRE(meta.type == PluginType::NotificationHandler);
        REQUIRE(meta.capabilities.size() == 1);
        REQUIRE(meta.dependencies.size() == 1);
        REQUIRE(meta.dependencies[0].required == false);
    }

    SECTION("Full ID generation") {
        PluginMetadata meta;
        meta.id = "com.test.plugin";
        meta.version = "1.2.3";

        REQUIRE(meta.fullId() == "com.test.plugin@1.2.3");
    }
}

TEST_CASE("PluginType and PluginState conversion", "[Plugin]") {
    SECTION("PluginType to string") {
        REQUIRE(pluginTypeToString(PluginType::NetworkMonitor) == "NetworkMonitor");
        REQUIRE(pluginTypeToString(PluginType::NotificationHandler) == "NotificationHandler");
        REQUIRE(pluginTypeToString(PluginType::DataProcessor) == "DataProcessor");
        REQUIRE(pluginTypeToString(PluginType::DataExporter) == "DataExporter");
        REQUIRE(pluginTypeToString(PluginType::StorageBackend) == "StorageBackend");
        REQUIRE(pluginTypeToString(PluginType::Widget) == "Widget");
    }

    SECTION("PluginState to string") {
        REQUIRE(pluginStateToString(PluginState::Unloaded) == "Unloaded");
        REQUIRE(pluginStateToString(PluginState::Loaded) == "Loaded");
        REQUIRE(pluginStateToString(PluginState::Initialized) == "Initialized");
        REQUIRE(pluginStateToString(PluginState::Running) == "Running");
        REQUIRE(pluginStateToString(PluginState::Stopped) == "Stopped");
        REQUIRE(pluginStateToString(PluginState::Error) == "Error");
    }
}

TEST_CASE("PluginContext service registration", "[Plugin]") {
    PluginContext context("/tmp/test/config", "/tmp/test/data", "/tmp/test/plugins", "1.0.0");

    SECTION("Register and retrieve service") {
        int testService = 42;
        context.registerService("testService", &testService);

        REQUIRE(context.hasService("testService"));
        auto* retrieved = static_cast<int*>(context.getService("testService"));
        REQUIRE(retrieved != nullptr);
        REQUIRE(*retrieved == 42);
    }

    SECTION("Unregister service") {
        int testService = 42;
        context.registerService("testService", &testService);
        REQUIRE(context.hasService("testService"));

        context.unregisterService("testService");
        REQUIRE_FALSE(context.hasService("testService"));
        REQUIRE(context.getService("testService") == nullptr);
    }

    SECTION("Non-existent service returns null") {
        REQUIRE(context.getService("nonexistent") == nullptr);
        REQUIRE_FALSE(context.hasService("nonexistent"));
    }
}

TEST_CASE("PluginContext event system", "[Plugin]") {
    PluginContext context("/tmp/test/config", "/tmp/test/data", "/tmp/test/plugins", "1.0.0");

    SECTION("Subscribe and publish") {
        int callCount = 0;
        std::string receivedEvent;

        auto id = context.subscribe("test.event", [&](const std::string& event, const std::any& data) {
            callCount++;
            receivedEvent = event;
        });

        context.publish("test.event", std::any{42});

        REQUIRE(callCount == 1);
        REQUIRE(receivedEvent == "test.event");
    }

    SECTION("Unsubscribe prevents callback") {
        int callCount = 0;

        auto id = context.subscribe("test.event", [&](const std::string&, const std::any&) {
            callCount++;
        });

        context.unsubscribe(id);
        context.publish("test.event", std::any{});

        REQUIRE(callCount == 0);
    }

    SECTION("Multiple subscribers") {
        int callCount1 = 0;
        int callCount2 = 0;

        context.subscribe("test.event", [&](const std::string&, const std::any&) {
            callCount1++;
        });

        context.subscribe("test.event", [&](const std::string&, const std::any&) {
            callCount2++;
        });

        context.publish("test.event", std::any{});

        REQUIRE(callCount1 == 1);
        REQUIRE(callCount2 == 1);
    }

    SECTION("Event filtering by name") {
        int callCount = 0;

        context.subscribe("event.a", [&](const std::string&, const std::any&) {
            callCount++;
        });

        context.publish("event.b", std::any{});

        REQUIRE(callCount == 0);
    }
}

TEST_CASE("PluginContext directories and version", "[Plugin]") {
    PluginContext context("/config/path", "/data/path", "/plugins/path", "2.5.0");

    REQUIRE(context.configDir() == "/config/path");
    REQUIRE(context.dataDir() == "/data/path");
    REQUIRE(context.pluginDir() == "/plugins/path");
    REQUIRE(context.appVersion() == "2.5.0");
}

TEST_CASE("MockPlugin lifecycle", "[Plugin]") {
    MockPlugin plugin;
    PluginContext context("/tmp", "/tmp", "/tmp", "1.0.0");

    SECTION("Initial state") {
        REQUIRE(plugin.state() == PluginState::Loaded);
        REQUIRE(plugin.metadata().id == "com.test.mock");
        REQUIRE(plugin.isEnabled());
    }

    SECTION("Initialize") {
        REQUIRE(plugin.initialize(&context));
        REQUIRE(plugin.initializeCalled_);
        REQUIRE(plugin.state() == PluginState::Initialized);
        REQUIRE(plugin.isHealthy());
    }

    SECTION("Initialize with null context fails") {
        REQUIRE_FALSE(plugin.initialize(nullptr));
    }

    SECTION("Shutdown") {
        plugin.initialize(&context);
        plugin.shutdown();

        REQUIRE(plugin.shutdownCalled_);
        REQUIRE(plugin.state() == PluginState::Stopped);
        REQUIRE_FALSE(plugin.isHealthy());
    }

    SECTION("Configure") {
        nlohmann::json config = {{"key", "value"}};
        plugin.configure(config);

        REQUIRE(plugin.configureCalled_);
        REQUIRE(plugin.configuration()["key"] == "value");
    }

    SECTION("Enable and disable") {
        plugin.disable();
        REQUIRE_FALSE(plugin.isEnabled());

        plugin.enable();
        REQUIRE(plugin.isEnabled());
    }
}

TEST_CASE("NetworkMonitorResult structure", "[Plugin]") {
    NetworkMonitorResult result;
    result.monitorType = "icmp";
    result.targetAddress = "192.168.1.1";
    result.success = true;
    result.latency = std::chrono::milliseconds(25);
    result.timestamp = std::chrono::system_clock::now();

    REQUIRE(result.monitorType == "icmp");
    REQUIRE(result.targetAddress == "192.168.1.1");
    REQUIRE(result.success);
    REQUIRE(result.latency.count() == 25);
}

TEST_CASE("NotificationPayload structure", "[Plugin]") {
    NotificationPayload payload;
    payload.title = "Alert";
    payload.message = "Host down";
    payload.severity = "critical";
    payload.metadata = {{"hostId", 123}};
    payload.timestamp = std::chrono::system_clock::now();

    REQUIRE(payload.title == "Alert");
    REQUIRE(payload.message == "Host down");
    REQUIRE(payload.severity == "critical");
    REQUIRE(payload.metadata["hostId"] == 123);
}

TEST_CASE("ProcessedData structure", "[Plugin]") {
    ProcessedData data;
    data.dataType = "ping_result";
    data.originalData = {{"latency", 50}};
    data.enrichedData = {{"latency", 50}, {"processed", true}};
    data.tags = {"tag1", "tag2"};
    data.processedAt = std::chrono::system_clock::now();

    REQUIRE(data.dataType == "ping_result");
    REQUIRE(data.originalData["latency"] == 50);
    REQUIRE(data.enrichedData["processed"] == true);
    REQUIRE(data.tags.size() == 2);
}

TEST_CASE("ExportResult structure", "[Plugin]") {
    ExportResult result;
    result.success = true;
    result.recordsExported = 100;
    result.duration = std::chrono::milliseconds(500);

    REQUIRE(result.success);
    REQUIRE(result.recordsExported == 100);
    REQUIRE(result.duration.count() == 500);
}
