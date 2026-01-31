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

struct PluginInfo {
    core::PluginMetadata metadata;
    std::filesystem::path path;
    bool enabled{true};
    nlohmann::json configuration;
};

struct LoadedPlugin {
    std::shared_ptr<core::IPlugin> instance;
    void* handle{nullptr};
    std::filesystem::path path;
    bool ownsHandle{true};
};

class PluginManager {
public:
    using PluginLoadedCallback = std::function<void(const std::string&, core::IPlugin*)>;
    using PluginUnloadedCallback = std::function<void(const std::string&)>;
    using PluginErrorCallback = std::function<void(const std::string&, const std::string&)>;

    explicit PluginManager(
        const std::filesystem::path& pluginDir,
        const std::filesystem::path& configDir,
        const std::filesystem::path& dataDir,
        const std::string& appVersion);

    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    bool loadPlugin(const std::filesystem::path& path);
    bool unloadPlugin(const std::string& pluginId);
    void unloadAllPlugins();

    bool initializePlugin(const std::string& pluginId);
    void initializeAllPlugins();
    void shutdownAllPlugins();

    void enablePlugin(const std::string& pluginId);
    void disablePlugin(const std::string& pluginId);

    [[nodiscard]] std::vector<PluginInfo> discoverPlugins() const;
    void refreshPluginList();

    [[nodiscard]] std::shared_ptr<core::IPlugin> getPlugin(const std::string& pluginId) const;
    [[nodiscard]] std::vector<std::shared_ptr<core::IPlugin>> getPlugins() const;
    [[nodiscard]] std::vector<std::shared_ptr<core::IPlugin>> getPluginsByType(core::PluginType type) const;

    [[nodiscard]] bool isLoaded(const std::string& pluginId) const;
    [[nodiscard]] core::PluginState getPluginState(const std::string& pluginId) const;
    [[nodiscard]] std::vector<std::string> getLoadedPluginIds() const;

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

    [[nodiscard]] std::vector<core::INetworkMonitorPlugin*> getNetworkMonitors() const;
    [[nodiscard]] std::vector<core::INotificationPlugin*> getNotificationHandlers() const;
    [[nodiscard]] std::vector<core::IDataProcessorPlugin*> getDataProcessors() const;
    [[nodiscard]] std::vector<core::IDataExporterPlugin*> getDataExporters() const;

    void setPluginConfig(const std::string& pluginId, const nlohmann::json& config);
    [[nodiscard]] nlohmann::json getPluginConfig(const std::string& pluginId) const;

    bool savePluginStates(const std::filesystem::path& path) const;
    bool loadPluginStates(const std::filesystem::path& path);

    void registerService(const std::string& serviceName, void* service);
    void unregisterService(const std::string& serviceName);

    int64_t subscribe(const std::string& eventName, core::IPluginContext::EventCallback callback);
    void unsubscribe(int64_t subscriptionId);
    void publish(const std::string& eventName, const std::any& data);

    void onPluginLoaded(PluginLoadedCallback callback);
    void onPluginUnloaded(PluginUnloadedCallback callback);
    void onPluginError(PluginErrorCallback callback);

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
