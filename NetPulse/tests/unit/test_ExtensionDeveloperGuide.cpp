/**
 * @file test_ExtensionDeveloperGuide.cpp
 * @brief Tests validating the Extension Developer Guide code examples.
 *
 * These tests ensure that the code examples in the Extension Developer Guide
 * compile and function as documented.
 */

#include <catch2/catch_test_macros.hpp>

#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IPluginContext.hpp"
#include "core/plugin/PluginHooks.hpp"
#include "infrastructure/plugin/PluginContext.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

using namespace netpulse::core;
using namespace netpulse::infra;

/**
 * @brief Minimal plugin implementation as shown in the developer guide.
 *
 * This validates that the minimal example compiles and works correctly.
 */
class MinimalPlugin : public IPlugin {
public:
    MinimalPlugin() {
        metadata_.id = "com.example.minimal";
        metadata_.name = "Minimal Plugin";
        metadata_.version = "1.0.0";
        metadata_.author = "Test Author";
        metadata_.description = "Minimal plugin for guide validation";
        metadata_.license = "MIT";
        metadata_.type = PluginType::DataProcessor;
        metadata_.minAppVersion = "1.0.0";

        state_ = PluginState::Loaded;
    }

    ~MinimalPlugin() override {
        if (state_ >= PluginState::Initialized) {
            shutdown();
        }
    }

    [[nodiscard]] const PluginMetadata& metadata() const override {
        return metadata_;
    }

    [[nodiscard]] PluginState state() const override {
        return state_;
    }

    bool initialize(IPluginContext* context) override {
        if (!context) {
            return false;
        }

        context_ = context;
        state_ = PluginState::Initialized;
        config_ = defaultConfiguration();
        context_->logInfo("MinimalPlugin initialized successfully");
        state_ = PluginState::Running;
        return true;
    }

    void shutdown() override {
        if (context_) {
            context_->logInfo("MinimalPlugin shutting down");
        }
        state_ = PluginState::Stopped;
        context_ = nullptr;
    }

    void configure(const nlohmann::json& config) override {
        config_ = config;
        if (context_) {
            context_->logInfo("MinimalPlugin configuration updated");
        }
    }

    [[nodiscard]] nlohmann::json configuration() const override {
        return config_;
    }

    [[nodiscard]] nlohmann::json defaultConfiguration() const override {
        return {
            {"enabled", true},
            {"logLevel", "info"}
        };
    }

    [[nodiscard]] bool isHealthy() const override {
        return state_ == PluginState::Running;
    }

    [[nodiscard]] std::string statusMessage() const override {
        return pluginStateToString(state_);
    }

    [[nodiscard]] nlohmann::json diagnostics() const override {
        return {
            {"state", pluginStateToString(state_)},
            {"enabled", enabled_}
        };
    }

    void enable() override {
        enabled_ = true;
    }

    void disable() override {
        enabled_ = false;
    }

    [[nodiscard]] bool isEnabled() const override {
        return enabled_;
    }

private:
    PluginMetadata metadata_;
    PluginState state_{PluginState::Unloaded};
    IPluginContext* context_{nullptr};
    nlohmann::json config_;
    bool enabled_{true};
};

/**
 * @brief Data processor plugin as shown in the developer guide.
 *
 * Validates the IDataProcessorPlugin interface example.
 */
class DataEnricherPlugin : public IPlugin, public IDataProcessorPlugin {
public:
    DataEnricherPlugin() {
        metadata_.id = "com.example.dataenricher";
        metadata_.name = "Data Enricher Plugin";
        metadata_.version = "1.0.0";
        metadata_.author = "Test Author";
        metadata_.description = "Data enrichment plugin for guide validation";
        metadata_.license = "MIT";
        metadata_.type = PluginType::DataProcessor;
        metadata_.minAppVersion = "1.0.0";

        metadata_.capabilities.push_back({
            "data-processor",
            "1.0.0",
            "Enriches monitoring data"
        });

        state_ = PluginState::Loaded;
    }

    ~DataEnricherPlugin() override {
        if (state_ >= PluginState::Initialized) {
            shutdown();
        }
    }

    // IPlugin interface
    [[nodiscard]] const PluginMetadata& metadata() const override { return metadata_; }
    [[nodiscard]] PluginState state() const override { return state_; }

    bool initialize(IPluginContext* context) override {
        if (!context) return false;
        context_ = context;
        state_ = PluginState::Initialized;
        config_ = defaultConfiguration();
        state_ = PluginState::Running;
        return true;
    }

    void shutdown() override {
        state_ = PluginState::Stopped;
        context_ = nullptr;
    }

    void configure(const nlohmann::json& config) override { config_ = config; }
    [[nodiscard]] nlohmann::json configuration() const override { return config_; }
    [[nodiscard]] nlohmann::json defaultConfiguration() const override {
        return {{"enabled", true}};
    }

    [[nodiscard]] bool isHealthy() const override { return state_ == PluginState::Running; }
    [[nodiscard]] std::string statusMessage() const override { return pluginStateToString(state_); }
    [[nodiscard]] nlohmann::json diagnostics() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            {"state", pluginStateToString(state_)},
            {"processedCount", processedCount_}
        };
    }

    void enable() override { enabled_ = true; }
    void disable() override { enabled_ = false; }
    [[nodiscard]] bool isEnabled() const override { return enabled_.load(); }

    // IDataProcessorPlugin interface
    [[nodiscard]] std::vector<std::string> supportedDataTypes() const override {
        return {"ping_result", "alert"};
    }

    ProcessedData process(const std::string& dataType, const nlohmann::json& data) override {
        ProcessedData result;
        result.dataType = dataType;
        result.originalData = data;
        result.processedAt = std::chrono::system_clock::now();

        result.enrichedData = data;
        result.enrichedData["enriched_by"] = metadata_.id;
        result.tags = {"enriched", dataType};

        {
            std::lock_guard<std::mutex> lock(mutex_);
            processedCount_++;
        }

        return result;
    }

    [[nodiscard]] bool canProcess(const std::string& dataType) const override {
        auto types = supportedDataTypes();
        return std::find(types.begin(), types.end(), dataType) != types.end();
    }

    void onPingResult(const PingResult& result) override {
        std::lock_guard<std::mutex> lock(mutex_);
        pingResultsReceived_++;
    }

    void onAlert(const Alert& alert) override {
        std::lock_guard<std::mutex> lock(mutex_);
        alertsReceived_++;
    }

    int64_t getProcessedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return processedCount_;
    }

    int64_t getPingResultsReceived() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pingResultsReceived_;
    }

    int64_t getAlertsReceived() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return alertsReceived_;
    }

private:
    PluginMetadata metadata_;
    PluginState state_{PluginState::Unloaded};
    IPluginContext* context_{nullptr};
    nlohmann::json config_;
    std::atomic<bool> enabled_{true};

    mutable std::mutex mutex_;
    int64_t processedCount_{0};
    int64_t pingResultsReceived_{0};
    int64_t alertsReceived_{0};
};

TEST_CASE("Extension Developer Guide: Minimal Plugin Example", "[ExtensionGuide]") {
    PluginContext context("/tmp/config", "/tmp/data", "/tmp/plugins", "1.0.0");

    SECTION("Plugin metadata is correctly set") {
        MinimalPlugin plugin;

        REQUIRE(plugin.metadata().id == "com.example.minimal");
        REQUIRE(plugin.metadata().name == "Minimal Plugin");
        REQUIRE(plugin.metadata().version == "1.0.0");
        REQUIRE(plugin.metadata().type == PluginType::DataProcessor);
        REQUIRE(plugin.metadata().minAppVersion == "1.0.0");
    }

    SECTION("Plugin lifecycle works as documented") {
        MinimalPlugin plugin;

        // Initial state is Loaded
        REQUIRE(plugin.state() == PluginState::Loaded);
        REQUIRE_FALSE(plugin.isHealthy());

        // Initialize transitions to Running
        REQUIRE(plugin.initialize(&context));
        REQUIRE(plugin.state() == PluginState::Running);
        REQUIRE(plugin.isHealthy());

        // Shutdown transitions to Stopped
        plugin.shutdown();
        REQUIRE(plugin.state() == PluginState::Stopped);
        REQUIRE_FALSE(plugin.isHealthy());
    }

    SECTION("Plugin configuration works as documented") {
        MinimalPlugin plugin;
        plugin.initialize(&context);

        // Default configuration
        auto defaultConfig = plugin.defaultConfiguration();
        REQUIRE(defaultConfig["enabled"] == true);
        REQUIRE(defaultConfig["logLevel"] == "info");

        // Update configuration
        nlohmann::json newConfig = {{"enabled", false}, {"logLevel", "debug"}};
        plugin.configure(newConfig);
        REQUIRE(plugin.configuration()["enabled"] == false);
        REQUIRE(plugin.configuration()["logLevel"] == "debug");
    }

    SECTION("Plugin enable/disable works as documented") {
        MinimalPlugin plugin;

        REQUIRE(plugin.isEnabled());
        plugin.disable();
        REQUIRE_FALSE(plugin.isEnabled());
        plugin.enable();
        REQUIRE(plugin.isEnabled());
    }

    SECTION("Plugin diagnostics returns expected format") {
        MinimalPlugin plugin;
        plugin.initialize(&context);

        auto diag = plugin.diagnostics();
        REQUIRE(diag.contains("state"));
        REQUIRE(diag.contains("enabled"));
        REQUIRE(diag["state"] == "Running");
        REQUIRE(diag["enabled"] == true);
    }

    SECTION("Null context returns false from initialize") {
        MinimalPlugin plugin;
        REQUIRE_FALSE(plugin.initialize(nullptr));
    }
}

TEST_CASE("Extension Developer Guide: Data Processor Example", "[ExtensionGuide]") {
    PluginContext context("/tmp/config", "/tmp/data", "/tmp/plugins", "1.0.0");

    SECTION("IDataProcessorPlugin interface works as documented") {
        DataEnricherPlugin plugin;
        plugin.initialize(&context);

        // supportedDataTypes returns correct types
        auto types = plugin.supportedDataTypes();
        REQUIRE(types.size() == 2);
        REQUIRE(std::find(types.begin(), types.end(), "ping_result") != types.end());
        REQUIRE(std::find(types.begin(), types.end(), "alert") != types.end());

        // canProcess checks type correctly
        REQUIRE(plugin.canProcess("ping_result"));
        REQUIRE(plugin.canProcess("alert"));
        REQUIRE_FALSE(plugin.canProcess("unknown_type"));
    }

    SECTION("process() enriches data as documented") {
        DataEnricherPlugin plugin;
        plugin.initialize(&context);

        nlohmann::json inputData = {{"latency", 50}, {"success", true}};
        auto result = plugin.process("ping_result", inputData);

        // Result structure is correct
        REQUIRE(result.dataType == "ping_result");
        REQUIRE(result.originalData == inputData);
        REQUIRE(result.enrichedData.contains("enriched_by"));
        REQUIRE(result.enrichedData["enriched_by"] == "com.example.dataenricher");
        REQUIRE(result.tags.size() == 2);
        REQUIRE(std::find(result.tags.begin(), result.tags.end(), "enriched") != result.tags.end());
        REQUIRE(std::find(result.tags.begin(), result.tags.end(), "ping_result") != result.tags.end());

        // Process count incremented
        REQUIRE(plugin.getProcessedCount() == 1);
    }

    SECTION("Event hooks are called correctly") {
        DataEnricherPlugin plugin;
        plugin.initialize(&context);

        // Call onPingResult
        PingResult pingResult;
        pingResult.hostId = 1;
        pingResult.success = true;
        pingResult.latency = std::chrono::microseconds(25000);  // 25ms
        plugin.onPingResult(pingResult);
        REQUIRE(plugin.getPingResultsReceived() == 1);

        // Call onAlert
        Alert alert;
        alert.id = 1;
        alert.hostId = 1;
        alert.severity = AlertSeverity::Warning;
        alert.message = "Test alert";
        plugin.onAlert(alert);
        REQUIRE(plugin.getAlertsReceived() == 1);
    }

    SECTION("Thread safety with atomic and mutex") {
        DataEnricherPlugin plugin;
        plugin.initialize(&context);

        // Enable/disable uses atomic
        plugin.disable();
        REQUIRE_FALSE(plugin.isEnabled());
        plugin.enable();
        REQUIRE(plugin.isEnabled());

        // Process uses mutex for count
        nlohmann::json data = {{"test", true}};
        plugin.process("ping_result", data);
        plugin.process("ping_result", data);
        plugin.process("ping_result", data);
        REQUIRE(plugin.getProcessedCount() == 3);
    }
}

TEST_CASE("Extension Developer Guide: Plugin Context Usage", "[ExtensionGuide]") {
    PluginContext context("/config/path", "/data/path", "/plugins/path", "2.0.0");

    SECTION("Directory paths are accessible as documented") {
        REQUIRE(context.configDir() == "/config/path");
        REQUIRE(context.dataDir() == "/data/path");
        REQUIRE(context.pluginDir() == "/plugins/path");
    }

    SECTION("App version is accessible as documented") {
        REQUIRE(context.appVersion() == "2.0.0");
    }

    SECTION("Service registration works as documented") {
        int testService = 42;
        context.registerService("myService", &testService);

        REQUIRE(context.hasService("myService"));
        auto* retrieved = static_cast<int*>(context.getService("myService"));
        REQUIRE(retrieved != nullptr);
        REQUIRE(*retrieved == 42);

        context.unregisterService("myService");
        REQUIRE_FALSE(context.hasService("myService"));
    }

    SECTION("Event system works as documented") {
        int callCount = 0;
        nlohmann::json receivedData;

        // Subscribe to event
        int64_t subscriptionId = context.subscribe("myplugin.data.processed",
            [&](const std::string& eventName, const std::any& data) {
                callCount++;
                receivedData = std::any_cast<nlohmann::json>(data);
            });

        // Publish event
        nlohmann::json eventData = {{"hostId", 123}, {"status", "online"}};
        context.publish("myplugin.data.processed", eventData);

        REQUIRE(callCount == 1);
        REQUIRE(receivedData["hostId"] == 123);
        REQUIRE(receivedData["status"] == "online");

        // Unsubscribe
        context.unsubscribe(subscriptionId);
        context.publish("myplugin.data.processed", eventData);
        REQUIRE(callCount == 1);  // Should not increase
    }
}

TEST_CASE("Extension Developer Guide: Plugin Metadata Structures", "[ExtensionGuide]") {
    SECTION("PluginCapability structure") {
        PluginCapability cap;
        cap.name = "data-processor";
        cap.version = "1.0.0";
        cap.description = "Processes monitoring data";

        REQUIRE(cap.name == "data-processor");
        REQUIRE(cap.version == "1.0.0");
        REQUIRE(cap.description == "Processes monitoring data");
    }

    SECTION("PluginDependency structure") {
        PluginDependency dep;
        dep.pluginName = "com.netpulse.core";
        dep.minVersion = "1.0.0";
        dep.required = true;

        REQUIRE(dep.pluginName == "com.netpulse.core");
        REQUIRE(dep.minVersion == "1.0.0");
        REQUIRE(dep.required == true);
    }

    SECTION("PluginMetadata fullId() format") {
        PluginMetadata meta;
        meta.id = "com.example.plugin";
        meta.version = "1.2.3";

        REQUIRE(meta.fullId() == "com.example.plugin@1.2.3");
    }

    SECTION("PluginMetadata JSON serialization") {
        PluginMetadata meta;
        meta.id = "com.example.plugin";
        meta.name = "Example Plugin";
        meta.version = "1.0.0";
        meta.author = "Test Author";
        meta.description = "Test description";
        meta.license = "MIT";
        meta.type = PluginType::DataProcessor;
        meta.minAppVersion = "1.0.0";
        meta.capabilities.push_back({"cap1", "1.0.0", "Capability 1"});
        meta.dependencies.push_back({"other.plugin", "1.0.0", true});

        auto json = meta.toJson();

        REQUIRE(json["id"] == "com.example.plugin");
        REQUIRE(json["name"] == "Example Plugin");
        REQUIRE(json["version"] == "1.0.0");
        REQUIRE(json["author"] == "Test Author");
        REQUIRE(json["description"] == "Test description");
        REQUIRE(json["license"] == "MIT");
        REQUIRE(json["type"] == static_cast<int>(PluginType::DataProcessor));
        REQUIRE(json["minAppVersion"] == "1.0.0");
        REQUIRE(json["capabilities"].size() == 1);
        REQUIRE(json["dependencies"].size() == 1);
    }

    SECTION("PluginMetadata JSON deserialization") {
        nlohmann::json json = {
            {"id", "com.example.loaded"},
            {"name", "Loaded Plugin"},
            {"version", "2.0.0"},
            {"author", "Author"},
            {"description", "Description"},
            {"license", "Apache-2.0"},
            {"type", static_cast<int>(PluginType::NotificationHandler)},
            {"minAppVersion", "1.5.0"},
            {"capabilities", {{{"name", "notifier"}, {"version", "1.0"}, {"description", "Sends notifications"}}}},
            {"dependencies", {{{"pluginName", "core"}, {"minVersion", "1.0"}, {"required", false}}}}
        };

        auto meta = PluginMetadata::fromJson(json);

        REQUIRE(meta.id == "com.example.loaded");
        REQUIRE(meta.name == "Loaded Plugin");
        REQUIRE(meta.version == "2.0.0");
        REQUIRE(meta.type == PluginType::NotificationHandler);
        REQUIRE(meta.minAppVersion == "1.5.0");
        REQUIRE(meta.capabilities.size() == 1);
        REQUIRE(meta.capabilities[0].name == "notifier");
        REQUIRE(meta.dependencies.size() == 1);
        REQUIRE(meta.dependencies[0].required == false);
    }
}

TEST_CASE("Extension Developer Guide: Plugin Type Enumeration", "[ExtensionGuide]") {
    SECTION("All plugin types have string representations") {
        REQUIRE(pluginTypeToString(PluginType::NetworkMonitor) == "NetworkMonitor");
        REQUIRE(pluginTypeToString(PluginType::NotificationHandler) == "NotificationHandler");
        REQUIRE(pluginTypeToString(PluginType::DataProcessor) == "DataProcessor");
        REQUIRE(pluginTypeToString(PluginType::DataExporter) == "DataExporter");
        REQUIRE(pluginTypeToString(PluginType::StorageBackend) == "StorageBackend");
        REQUIRE(pluginTypeToString(PluginType::Widget) == "Widget");
    }

    SECTION("Plugin type values match documentation") {
        REQUIRE(static_cast<int>(PluginType::NetworkMonitor) == 0);
        REQUIRE(static_cast<int>(PluginType::NotificationHandler) == 1);
        REQUIRE(static_cast<int>(PluginType::DataProcessor) == 2);
        REQUIRE(static_cast<int>(PluginType::DataExporter) == 3);
        REQUIRE(static_cast<int>(PluginType::StorageBackend) == 4);
        REQUIRE(static_cast<int>(PluginType::Widget) == 5);
    }
}

TEST_CASE("Extension Developer Guide: Plugin State Enumeration", "[ExtensionGuide]") {
    SECTION("All plugin states have string representations") {
        REQUIRE(pluginStateToString(PluginState::Unloaded) == "Unloaded");
        REQUIRE(pluginStateToString(PluginState::Loaded) == "Loaded");
        REQUIRE(pluginStateToString(PluginState::Initialized) == "Initialized");
        REQUIRE(pluginStateToString(PluginState::Running) == "Running");
        REQUIRE(pluginStateToString(PluginState::Stopped) == "Stopped");
        REQUIRE(pluginStateToString(PluginState::Error) == "Error");
    }
}

TEST_CASE("Extension Developer Guide: Data Structures", "[ExtensionGuide]") {
    SECTION("NetworkMonitorResult structure") {
        NetworkMonitorResult result;
        result.monitorType = "http";
        result.targetAddress = "https://example.com";
        result.success = true;
        result.latency = std::chrono::milliseconds(150);
        result.timestamp = std::chrono::system_clock::now();
        result.additionalData = {{"statusCode", 200}};

        REQUIRE(result.monitorType == "http");
        REQUIRE(result.targetAddress == "https://example.com");
        REQUIRE(result.success);
        REQUIRE(result.latency.count() == 150);
        REQUIRE(result.additionalData["statusCode"] == 200);
    }

    SECTION("NotificationPayload structure") {
        NotificationPayload payload;
        payload.title = "Host Down";
        payload.message = "Server is not responding";
        payload.severity = "critical";
        payload.metadata = {{"hostId", 42}, {"region", "us-west"}};
        payload.timestamp = std::chrono::system_clock::now();

        REQUIRE(payload.title == "Host Down");
        REQUIRE(payload.message == "Server is not responding");
        REQUIRE(payload.severity == "critical");
        REQUIRE(payload.metadata["hostId"] == 42);
        REQUIRE(payload.metadata["region"] == "us-west");
    }

    SECTION("PluginNotificationResult structure") {
        PluginNotificationResult result;
        result.success = true;
        result.httpStatusCode = 200;
        result.deliveryTime = std::chrono::milliseconds(50);

        REQUIRE(result.success);
        REQUIRE(result.httpStatusCode == 200);
        REQUIRE(result.deliveryTime.count() == 50);
    }

    SECTION("ProcessedData structure") {
        ProcessedData data;
        data.dataType = "ping_result";
        data.originalData = {{"latency", 25}};
        data.enrichedData = {{"latency", 25}, {"region", "us-east"}};
        data.tags = {"enriched", "geo-tagged"};
        data.processedAt = std::chrono::system_clock::now();

        REQUIRE(data.dataType == "ping_result");
        REQUIRE(data.originalData["latency"] == 25);
        REQUIRE(data.enrichedData["region"] == "us-east");
        REQUIRE(data.tags.size() == 2);
    }

    SECTION("ExportResult structure") {
        ExportResult result;
        result.success = true;
        result.recordsExported = 1000;
        result.duration = std::chrono::milliseconds(250);

        REQUIRE(result.success);
        REQUIRE(result.recordsExported == 1000);
        REQUIRE(result.duration.count() == 250);
    }
}
