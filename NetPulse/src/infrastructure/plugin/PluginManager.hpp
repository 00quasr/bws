#pragma once

#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IPluginContext.hpp"
#include "core/plugin/PluginHooks.hpp"
#include "infrastructure/plugin/PluginContext.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Metadata about an available plugin.
 */
struct PluginInfo {
    core::PluginMetadata metadata; ///< Plugin metadata from the plugin.
    std::filesystem::path path;    ///< Path to the plugin shared library.
    bool enabled{true};            ///< Whether the plugin is enabled.
    nlohmann::json configuration;  ///< Plugin configuration.
};

/**
 * @brief State of a loaded plugin instance.
 */
struct LoadedPlugin {
    std::shared_ptr<core::IPlugin> instance; ///< Plugin instance.
    void* handle{nullptr};                   ///< Shared library handle.
    std::filesystem::path path;              ///< Path to the plugin file.
    bool ownsHandle{true};                   ///< Whether to close handle on unload.
};

/**
 * @brief Manages plugin discovery, loading, and lifecycle.
 *
 * Handles loading plugins from shared libraries, managing their lifecycle,
 * and providing access to plugin interfaces. Supports dependency resolution,
 * configuration persistence, and event callbacks.
 *
 * @note This class is non-copyable.
 */
class PluginManager {
public:
    /** @brief Callback type for plugin load events. */
    using PluginLoadedCallback = std::function<void(const std::string&, core::IPlugin*)>;
    /** @brief Callback type for plugin unload events. */
    using PluginUnloadedCallback = std::function<void(const std::string&)>;
    /** @brief Callback type for plugin error events. */
    using PluginErrorCallback = std::function<void(const std::string&, const std::string&)>;

    /**
     * @brief Constructs a PluginManager.
     * @param pluginDir Path to the plugins directory.
     * @param configDir Path to the configuration directory.
     * @param dataDir Path to the data directory.
     * @param appVersion Application version for compatibility checking.
     */
    explicit PluginManager(
        const std::filesystem::path& pluginDir,
        const std::filesystem::path& configDir,
        const std::filesystem::path& dataDir,
        const std::string& appVersion);

    /**
     * @brief Destructor. Shuts down and unloads all plugins.
     */
    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * @brief Loads a plugin from a shared library file.
     * @param path Path to the plugin shared library.
     * @return True if loaded successfully, false otherwise.
     */
    bool loadPlugin(const std::filesystem::path& path);

    /**
     * @brief Unloads a plugin by its ID.
     * @param pluginId Unique identifier of the plugin.
     * @return True if unloaded successfully, false otherwise.
     */
    bool unloadPlugin(const std::string& pluginId);

    /**
     * @brief Unloads all loaded plugins.
     */
    void unloadAllPlugins();

    /**
     * @brief Initializes a loaded plugin.
     * @param pluginId Unique identifier of the plugin.
     * @return True if initialized successfully, false otherwise.
     */
    bool initializePlugin(const std::string& pluginId);

    /**
     * @brief Initializes all loaded plugins.
     */
    void initializeAllPlugins();

    /**
     * @brief Shuts down all loaded plugins.
     */
    void shutdownAllPlugins();

    /**
     * @brief Enables a plugin.
     * @param pluginId Unique identifier of the plugin.
     */
    void enablePlugin(const std::string& pluginId);

    /**
     * @brief Disables a plugin.
     * @param pluginId Unique identifier of the plugin.
     */
    void disablePlugin(const std::string& pluginId);

    /**
     * @brief Discovers available plugins in the plugins directory.
     * @return Vector of PluginInfo for all discovered plugins.
     */
    [[nodiscard]] std::vector<PluginInfo> discoverPlugins() const;

    /**
     * @brief Refreshes the list of available plugins.
     */
    void refreshPluginList();

    /**
     * @brief Retrieves a plugin by its ID.
     * @param pluginId Unique identifier of the plugin.
     * @return Shared pointer to the plugin, or nullptr if not found.
     */
    [[nodiscard]] std::shared_ptr<core::IPlugin> getPlugin(const std::string& pluginId) const;

    /**
     * @brief Retrieves all loaded plugins.
     * @return Vector of all loaded plugin instances.
     */
    [[nodiscard]] std::vector<std::shared_ptr<core::IPlugin>> getPlugins() const;

    /**
     * @brief Retrieves plugins of a specific type.
     * @param type Plugin type to filter by.
     * @return Vector of plugins matching the type.
     */
    [[nodiscard]] std::vector<std::shared_ptr<core::IPlugin>> getPluginsByType(core::PluginType type) const;

    /**
     * @brief Checks if a plugin is loaded.
     * @param pluginId Unique identifier of the plugin.
     * @return True if loaded, false otherwise.
     */
    [[nodiscard]] bool isLoaded(const std::string& pluginId) const;

    /**
     * @brief Gets the state of a plugin.
     * @param pluginId Unique identifier of the plugin.
     * @return Current PluginState.
     */
    [[nodiscard]] core::PluginState getPluginState(const std::string& pluginId) const;

    /**
     * @brief Gets the IDs of all loaded plugins.
     * @return Vector of plugin IDs.
     */
    [[nodiscard]] std::vector<std::string> getLoadedPluginIds() const;

    /**
     * @brief Retrieves plugins implementing a specific interface.
     * @tparam T Interface type to filter by.
     * @return Vector of pointers to plugins implementing T.
     */
    template<typename T>
    std::vector<T*> getPluginsWithInterface() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T*> result;
        for (const auto& [id, loaded] : loadedPlugins_) {
            if (auto* p = dynamic_cast<T*>(loaded.instance.get())) {
                result.push_back(p);
            }
        }
        return result;
    }

    /**
     * @brief Gets all network monitor plugins.
     * @return Vector of INetworkMonitorPlugin pointers.
     */
    [[nodiscard]] std::vector<core::INetworkMonitorPlugin*> getNetworkMonitors() const;

    /**
     * @brief Gets all notification handler plugins.
     * @return Vector of INotificationPlugin pointers.
     */
    [[nodiscard]] std::vector<core::INotificationPlugin*> getNotificationHandlers() const;

    /**
     * @brief Gets all data processor plugins.
     * @return Vector of IDataProcessorPlugin pointers.
     */
    [[nodiscard]] std::vector<core::IDataProcessorPlugin*> getDataProcessors() const;

    /**
     * @brief Gets all data exporter plugins.
     * @return Vector of IDataExporterPlugin pointers.
     */
    [[nodiscard]] std::vector<core::IDataExporterPlugin*> getDataExporters() const;

    /**
     * @brief Sets configuration for a plugin.
     * @param pluginId Unique identifier of the plugin.
     * @param config JSON configuration object.
     */
    void setPluginConfig(const std::string& pluginId, const nlohmann::json& config);

    /**
     * @brief Gets configuration for a plugin.
     * @param pluginId Unique identifier of the plugin.
     * @return JSON configuration object.
     */
    [[nodiscard]] nlohmann::json getPluginConfig(const std::string& pluginId) const;

    /**
     * @brief Saves plugin states to a file.
     * @param path Path to save the state file.
     * @return True if saved successfully, false otherwise.
     */
    bool savePluginStates(const std::filesystem::path& path) const;

    /**
     * @brief Loads plugin states from a file.
     * @param path Path to the state file.
     * @return True if loaded successfully, false otherwise.
     */
    bool loadPluginStates(const std::filesystem::path& path);

    /**
     * @brief Registers a service for plugin access.
     * @param serviceName Name to register the service under.
     * @param service Pointer to the service instance.
     */
    void registerService(const std::string& serviceName, void* service);

    /**
     * @brief Unregisters a service.
     * @param serviceName Name of the service to unregister.
     */
    void unregisterService(const std::string& serviceName);

    /**
     * @brief Subscribes to an event.
     * @param eventName Name of the event.
     * @param callback Function called when the event is published.
     * @return Subscription ID for later unsubscription.
     */
    int64_t subscribe(const std::string& eventName, core::IPluginContext::EventCallback callback);

    /**
     * @brief Unsubscribes from an event.
     * @param subscriptionId ID returned from subscribe().
     */
    void unsubscribe(int64_t subscriptionId);

    /**
     * @brief Publishes an event to all subscribers.
     * @param eventName Name of the event.
     * @param data Event data payload.
     */
    void publish(const std::string& eventName, const std::any& data);

    /**
     * @brief Registers a callback for plugin load events.
     * @param callback Function called when a plugin is loaded.
     */
    void onPluginLoaded(PluginLoadedCallback callback);

    /**
     * @brief Registers a callback for plugin unload events.
     * @param callback Function called when a plugin is unloaded.
     */
    void onPluginUnloaded(PluginUnloadedCallback callback);

    /**
     * @brief Registers a callback for plugin error events.
     * @param callback Function called when a plugin error occurs.
     */
    void onPluginError(PluginErrorCallback callback);

    /**
     * @brief Returns the plugin context.
     * @return Pointer to the IPluginContext.
     */
    [[nodiscard]] core::IPluginContext* context() const { return context_.get(); }

private:
    bool loadPluginFromSharedLibrary(const std::filesystem::path& path);
    bool validateDependencies(const core::PluginMetadata& metadata) const;
    bool isVersionCompatible(const std::string& required, const std::string& actual) const;
    void* openLibrary(const std::filesystem::path& path);
    void closeLibrary(void* handle);
    void* getSymbol(void* handle, const std::string& name);

    std::filesystem::path pluginDir_;
    std::filesystem::path configDir_;
    std::filesystem::path dataDir_;
    std::string appVersion_;

    mutable std::mutex mutex_;
    std::map<std::string, LoadedPlugin> loadedPlugins_;
    std::vector<PluginInfo> availablePlugins_;

    std::shared_ptr<PluginContext> context_;

    std::vector<PluginLoadedCallback> loadedCallbacks_;
    std::vector<PluginUnloadedCallback> unloadedCallbacks_;
    std::vector<PluginErrorCallback> errorCallbacks_;
};

} // namespace netpulse::infra
