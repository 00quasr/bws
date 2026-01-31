#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::core {

enum class PluginType : int {
    NetworkMonitor = 0,       // Custom network monitoring (ping, SNMP, etc.)
    NotificationHandler = 1,  // Custom notification/webhook handlers
    DataProcessor = 2,        // Data enrichment, correlation
    DataExporter = 3,         // Export to external systems
    StorageBackend = 4,       // Alternative database backends
    Widget = 5                // Custom dashboard widgets
};

enum class PluginState : int {
    Unloaded = 0,
    Loaded = 1,
    Initialized = 2,
    Running = 3,
    Stopped = 4,
    Error = 5
};

struct PluginCapability {
    std::string name;
    std::string version;
    std::string description;
};

struct PluginDependency {
    std::string pluginName;
    std::string minVersion;
    bool required{true};
};

struct PluginMetadata {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string license;
    PluginType type;
    std::vector<PluginCapability> capabilities;
    std::vector<PluginDependency> dependencies;
    std::string minAppVersion;

    [[nodiscard]] std::string fullId() const {
        return id + "@" + version;
    }

    [[nodiscard]] nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["version"] = version;
        j["author"] = author;
        j["description"] = description;
        j["license"] = license;
        j["type"] = static_cast<int>(type);
        j["minAppVersion"] = minAppVersion;

        j["capabilities"] = nlohmann::json::array();
        for (const auto& cap : capabilities) {
            j["capabilities"].push_back({
                {"name", cap.name},
                {"version", cap.version},
                {"description", cap.description}
            });
        }

        j["dependencies"] = nlohmann::json::array();
        for (const auto& dep : dependencies) {
            j["dependencies"].push_back({
                {"pluginName", dep.pluginName},
                {"minVersion", dep.minVersion},
                {"required", dep.required}
            });
        }

        return j;
    }

    static PluginMetadata fromJson(const nlohmann::json& j) {
        PluginMetadata meta;
        meta.id = j.value("id", "");
        meta.name = j.value("name", "");
        meta.version = j.value("version", "1.0.0");
        meta.author = j.value("author", "");
        meta.description = j.value("description", "");
        meta.license = j.value("license", "");
        meta.type = static_cast<PluginType>(j.value("type", 0));
        meta.minAppVersion = j.value("minAppVersion", "1.0.0");

        if (j.contains("capabilities") && j["capabilities"].is_array()) {
            for (const auto& cap : j["capabilities"]) {
                meta.capabilities.push_back({
                    cap.value("name", ""),
                    cap.value("version", "1.0.0"),
                    cap.value("description", "")
                });
            }
        }

        if (j.contains("dependencies") && j["dependencies"].is_array()) {
            for (const auto& dep : j["dependencies"]) {
                meta.dependencies.push_back({
                    dep.value("pluginName", ""),
                    dep.value("minVersion", "1.0.0"),
                    dep.value("required", true)
                });
            }
        }

        return meta;
    }
};

class IPluginContext;

class IPlugin {
public:
    virtual ~IPlugin() = default;

    [[nodiscard]] virtual const PluginMetadata& metadata() const = 0;
    [[nodiscard]] virtual PluginState state() const = 0;

    virtual bool initialize(IPluginContext* context) = 0;
    virtual void shutdown() = 0;

    virtual void configure(const nlohmann::json& config) = 0;
    [[nodiscard]] virtual nlohmann::json configuration() const = 0;
    [[nodiscard]] virtual nlohmann::json defaultConfiguration() const = 0;

    [[nodiscard]] virtual bool isHealthy() const = 0;
    [[nodiscard]] virtual std::string statusMessage() const = 0;
    [[nodiscard]] virtual nlohmann::json diagnostics() const = 0;

    virtual void enable() = 0;
    virtual void disable() = 0;
    [[nodiscard]] virtual bool isEnabled() const = 0;
};

inline std::string pluginTypeToString(PluginType type) {
    switch (type) {
        case PluginType::NetworkMonitor: return "NetworkMonitor";
        case PluginType::NotificationHandler: return "NotificationHandler";
        case PluginType::DataProcessor: return "DataProcessor";
        case PluginType::DataExporter: return "DataExporter";
        case PluginType::StorageBackend: return "StorageBackend";
        case PluginType::Widget: return "Widget";
        default: return "Unknown";
    }
}

inline std::string pluginStateToString(PluginState state) {
    switch (state) {
        case PluginState::Unloaded: return "Unloaded";
        case PluginState::Loaded: return "Loaded";
        case PluginState::Initialized: return "Initialized";
        case PluginState::Running: return "Running";
        case PluginState::Stopped: return "Stopped";
        case PluginState::Error: return "Error";
        default: return "Unknown";
    }
}

} // namespace netpulse::core
