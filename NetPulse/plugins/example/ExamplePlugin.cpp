#include "ExamplePlugin.hpp"

#include <chrono>

namespace netpulse::plugins {

ExamplePlugin::ExamplePlugin() {
    metadata_.id = "com.netpulse.example";
    metadata_.name = "Example Plugin";
    metadata_.version = "1.0.0";
    metadata_.author = "NetPulse Team";
    metadata_.description = "Example plugin demonstrating the plugin API";
    metadata_.license = "MIT";
    metadata_.type = core::PluginType::DataProcessor;
    metadata_.minAppVersion = "1.0.0";

    metadata_.capabilities.push_back({
        "data-processor",
        "1.0.0",
        "Processes ping results and alerts"
    });

    state_ = core::PluginState::Loaded;
}

ExamplePlugin::~ExamplePlugin() {
    if (state_ >= core::PluginState::Initialized) {
        shutdown();
    }
}

bool ExamplePlugin::initialize(core::IPluginContext* context) {
    if (!context) {
        return false;
    }

    context_ = context;
    state_ = core::PluginState::Initialized;

    context_->logInfo("ExamplePlugin initialized");

    config_ = defaultConfiguration();

    state_ = core::PluginState::Running;
    return true;
}

void ExamplePlugin::shutdown() {
    if (context_) {
        context_->logInfo("ExamplePlugin shutting down");
    }
    state_ = core::PluginState::Stopped;
    context_ = nullptr;
}

void ExamplePlugin::configure(const nlohmann::json& config) {
    config_ = config;
    if (context_) {
        context_->logInfo("ExamplePlugin reconfigured");
    }
}

nlohmann::json ExamplePlugin::defaultConfiguration() const {
    return {
        {"logLevel", "info"},
        {"processEnabled", true},
        {"tagPrefix", "example"}
    };
}

std::string ExamplePlugin::statusMessage() const {
    switch (state_) {
        case core::PluginState::Running:
            return "Running - processed " + std::to_string(processedCount_) + " items";
        case core::PluginState::Stopped:
            return "Stopped";
        case core::PluginState::Error:
            return "Error";
        default:
            return core::pluginStateToString(state_);
    }
}

nlohmann::json ExamplePlugin::diagnostics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return {
        {"state", core::pluginStateToString(state_)},
        {"enabled", enabled_.load()},
        {"processedCount", processedCount_},
        {"pingResultsReceived", pingResultsReceived_},
        {"alertsReceived", alertsReceived_}
    };
}

void ExamplePlugin::enable() {
    enabled_ = true;
    if (context_) {
        context_->logInfo("ExamplePlugin enabled");
    }
}

void ExamplePlugin::disable() {
    enabled_ = false;
    if (context_) {
        context_->logInfo("ExamplePlugin disabled");
    }
}

std::vector<std::string> ExamplePlugin::supportedDataTypes() const {
    return {"ping_result", "alert", "generic"};
}

core::ProcessedData ExamplePlugin::process(const std::string& dataType, const nlohmann::json& data) {
    core::ProcessedData result;
    result.dataType = dataType;
    result.originalData = data;
    result.processedAt = std::chrono::system_clock::now();

    if (!enabled_) {
        result.enrichedData = data;
        return result;
    }

    result.enrichedData = data;
    result.enrichedData["processed_by"] = metadata_.id;
    result.enrichedData["processed_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        result.processedAt.time_since_epoch()).count();

    std::string tagPrefix = config_.value("tagPrefix", "example");
    result.tags.push_back(tagPrefix + ":" + dataType);
    result.tags.push_back(tagPrefix + ":processed");

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        processedCount_++;
    }

    return result;
}

bool ExamplePlugin::canProcess(const std::string& dataType) const {
    auto supported = supportedDataTypes();
    return std::find(supported.begin(), supported.end(), dataType) != supported.end();
}

void ExamplePlugin::onPingResult(const core::PingResult& result) {
    if (!enabled_) return;

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        pingResultsReceived_++;
    }

    if (context_) {
        nlohmann::json data;
        data["hostId"] = result.hostId;
        data["success"] = result.success;
        data["latencyMs"] = result.latencyMs;

        context_->publish("example.ping_received", data);
    }
}

void ExamplePlugin::onAlert(const core::Alert& alert) {
    if (!enabled_) return;

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        alertsReceived_++;
    }

    if (context_) {
        nlohmann::json data;
        data["alertId"] = alert.id;
        data["hostId"] = alert.hostId;
        data["severity"] = static_cast<int>(alert.severity);
        data["message"] = alert.message;

        context_->publish("example.alert_received", data);
    }
}

} // namespace netpulse::plugins

extern "C" {

netpulse::core::IPlugin* netpulse_create_plugin() {
    return new netpulse::plugins::ExamplePlugin();
}

void netpulse_destroy_plugin(netpulse::core::IPlugin* plugin) {
    delete plugin;
}

}
