/**
 * @file IPlugin.hpp
 * @brief Plugin interface and metadata types.
 *
 * This file defines the base plugin interface and associated metadata
 * structures for the NetPulse plugin system.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::core {

/**
 * @brief Types of plugins supported by the system.
 */
enum class PluginType : int {
    NetworkMonitor = 0,      ///< Custom network monitoring (ping, SNMP, etc.)
    NotificationHandler = 1, ///< Custom notification/webhook handlers
    DataProcessor = 2,       ///< Data enrichment and correlation
    DataExporter = 3,        ///< Export to external systems
    StorageBackend = 4,      ///< Alternative database backends
    Widget = 5               ///< Custom dashboard widgets
};

/**
 * @brief Lifecycle states of a plugin.
 */
enum class PluginState : int {
    Unloaded = 0,    ///< Plugin is not loaded
    Loaded = 1,      ///< Plugin library is loaded but not initialized
    Initialized = 2, ///< Plugin has been initialized
    Running = 3,     ///< Plugin is actively running
    Stopped = 4,     ///< Plugin has been stopped
    Error = 5        ///< Plugin encountered an error
};

/**
 * @brief Describes a capability provided by a plugin.
 */
struct PluginCapability {
    std::string name;        ///< Name of the capability
    std::string version;     ///< Version of the capability
    std::string description; ///< Description of what the capability provides
};

/**
 * @brief Describes a dependency on another plugin.
 */
struct PluginDependency {
    std::string pluginName;  ///< Name of the required plugin
    std::string minVersion;  ///< Minimum required version
    bool required{true};     ///< Whether the dependency is mandatory
};

/**
 * @brief Metadata describing a plugin.
 *
 * Contains identification, versioning, and dependency information.
 */
struct PluginMetadata {
    std::string id;          ///< Unique plugin identifier
    std::string name;        ///< Human-readable plugin name
    std::string version;     ///< Plugin version string
    std::string author;      ///< Plugin author
    std::string description; ///< Plugin description
    std::string license;     ///< License type
    PluginType type;         ///< Type of plugin
    std::vector<PluginCapability> capabilities; ///< Capabilities provided
    std::vector<PluginDependency> dependencies; ///< Required dependencies
    std::string minAppVersion; ///< Minimum required application version

    /**
     * @brief Gets the fully qualified plugin identifier.
     * @return String in format "id@version".
     */
    [[nodiscard]] std::string fullId() const {
        return id + "@" + version;
    }

    /**
     * @brief Serializes the metadata to JSON.
     * @return JSON representation of the metadata.
     */
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

    /**
     * @brief Deserializes metadata from JSON.
     * @param j The JSON object to parse.
     * @return PluginMetadata populated from the JSON.
     */
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

/**
 * @brief Base interface for all plugins.
 *
 * Plugins must implement this interface to integrate with the NetPulse
 * plugin system. The interface provides lifecycle management, configuration,
 * and health monitoring capabilities.
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /**
     * @brief Gets the plugin metadata.
     * @return Reference to the plugin's metadata.
     */
    [[nodiscard]] virtual const PluginMetadata& metadata() const = 0;

    /**
     * @brief Gets the current plugin state.
     * @return The plugin's lifecycle state.
     */
    [[nodiscard]] virtual PluginState state() const = 0;

    /**
     * @brief Initializes the plugin.
     * @param context The plugin context for accessing services.
     * @return True if initialization succeeded.
     */
    virtual bool initialize(IPluginContext* context) = 0;

    /**
     * @brief Shuts down the plugin and releases resources.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Applies configuration to the plugin.
     * @param config JSON configuration object.
     */
    virtual void configure(const nlohmann::json& config) = 0;

    /**
     * @brief Gets the current plugin configuration.
     * @return JSON representation of current configuration.
     */
    [[nodiscard]] virtual nlohmann::json configuration() const = 0;

    /**
     * @brief Gets the default plugin configuration.
     * @return JSON representation of default configuration.
     */
    [[nodiscard]] virtual nlohmann::json defaultConfiguration() const = 0;

    /**
     * @brief Checks if the plugin is healthy and functioning.
     * @return True if the plugin is operating normally.
     */
    [[nodiscard]] virtual bool isHealthy() const = 0;

    /**
     * @brief Gets a human-readable status message.
     * @return Status message describing current state.
     */
    [[nodiscard]] virtual std::string statusMessage() const = 0;

    /**
     * @brief Gets diagnostic information for debugging.
     * @return JSON object with diagnostic data.
     */
    [[nodiscard]] virtual nlohmann::json diagnostics() const = 0;

    /**
     * @brief Enables the plugin.
     */
    virtual void enable() = 0;

    /**
     * @brief Disables the plugin.
     */
    virtual void disable() = 0;

    /**
     * @brief Checks if the plugin is enabled.
     * @return True if the plugin is enabled.
     */
    [[nodiscard]] virtual bool isEnabled() const = 0;
};

/**
 * @brief Converts a plugin type to its string representation.
 * @param type The plugin type to convert.
 * @return String representation of the type.
 */
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

/**
 * @brief Converts a plugin state to its string representation.
 * @param state The plugin state to convert.
 * @return String representation of the state.
 */
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
