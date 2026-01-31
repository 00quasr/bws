#include "infrastructure/plugin/PluginManager.hpp"
#include "infrastructure/plugin/PluginContext.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace netpulse::infra {

using CreatePluginFunc = core::IPlugin* (*)();
using DestroyPluginFunc = void (*)(core::IPlugin*);

PluginManager::PluginManager(
    const std::filesystem::path& pluginDir,
    const std::filesystem::path& configDir,
    const std::filesystem::path& dataDir,
    const std::string& appVersion)
    : pluginDir_(pluginDir)
    , configDir_(configDir)
    , dataDir_(dataDir)
    , appVersion_(appVersion) {

    std::filesystem::create_directories(pluginDir_);

    context_ = std::make_shared<PluginContext>(
        configDir_.string(),
        dataDir_.string(),
        pluginDir_.string(),
        appVersion_);

    spdlog::info("PluginManager initialized, plugin directory: {}", pluginDir_.string());
}

PluginManager::~PluginManager() {
    shutdownAllPlugins();
    unloadAllPlugins();
}

bool PluginManager::loadPlugin(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        spdlog::error("Plugin file not found: {}", path.string());
        return false;
    }

    return loadPluginFromSharedLibrary(path);
}

bool PluginManager::loadPluginFromSharedLibrary(const std::filesystem::path& path) {
    void* handle = openLibrary(path);
    if (!handle) {
        spdlog::error("Failed to open plugin library: {}", path.string());
        for (const auto& callback : errorCallbacks_) {
            callback(path.string(), "Failed to open library");
        }
        return false;
    }

    auto createFunc = reinterpret_cast<CreatePluginFunc>(getSymbol(handle, "netpulse_create_plugin"));
    if (!createFunc) {
        spdlog::error("Plugin missing netpulse_create_plugin function: {}", path.string());
        closeLibrary(handle);
        for (const auto& callback : errorCallbacks_) {
            callback(path.string(), "Missing create function");
        }
        return false;
    }

    core::IPlugin* rawPlugin = nullptr;
    try {
        rawPlugin = createFunc();
    } catch (const std::exception& e) {
        spdlog::error("Plugin creation failed: {} - {}", path.string(), e.what());
        closeLibrary(handle);
        for (const auto& callback : errorCallbacks_) {
            callback(path.string(), std::string("Creation failed: ") + e.what());
        }
        return false;
    }

    if (!rawPlugin) {
        spdlog::error("Plugin creation returned null: {}", path.string());
        closeLibrary(handle);
        for (const auto& callback : errorCallbacks_) {
            callback(path.string(), "Creation returned null");
        }
        return false;
    }

    const auto& metadata = rawPlugin->metadata();

    if (!validateDependencies(metadata)) {
        spdlog::error("Plugin dependency validation failed: {}", metadata.id);
        auto destroyFunc = reinterpret_cast<DestroyPluginFunc>(getSymbol(handle, "netpulse_destroy_plugin"));
        if (destroyFunc) {
            destroyFunc(rawPlugin);
        }
        closeLibrary(handle);
        for (const auto& callback : errorCallbacks_) {
            callback(metadata.id, "Dependency validation failed");
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (loadedPlugins_.find(metadata.id) != loadedPlugins_.end()) {
            spdlog::warn("Plugin already loaded: {}", metadata.id);
            auto destroyFunc = reinterpret_cast<DestroyPluginFunc>(getSymbol(handle, "netpulse_destroy_plugin"));
            if (destroyFunc) {
                destroyFunc(rawPlugin);
            }
            closeLibrary(handle);
            return false;
        }

        auto destroyFunc = reinterpret_cast<DestroyPluginFunc>(getSymbol(handle, "netpulse_destroy_plugin"));
        auto deleter = [destroyFunc, handle, this](core::IPlugin* p) {
            if (destroyFunc) {
                destroyFunc(p);
            }
        };

        LoadedPlugin loaded;
        loaded.instance = std::shared_ptr<core::IPlugin>(rawPlugin, deleter);
        loaded.handle = handle;
        loaded.path = path;
        loaded.ownsHandle = true;

        loadedPlugins_[metadata.id] = std::move(loaded);
    }

    spdlog::info("Plugin loaded: {} v{} ({})", metadata.name, metadata.version, metadata.id);

    for (const auto& callback : loadedCallbacks_) {
        callback(metadata.id, rawPlugin);
    }

    return true;
}

bool PluginManager::unloadPlugin(const std::string& pluginId) {
    LoadedPlugin loaded;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loadedPlugins_.find(pluginId);
        if (it == loadedPlugins_.end()) {
            spdlog::warn("Cannot unload plugin, not found: {}", pluginId);
            return false;
        }

        loaded = std::move(it->second);
        loadedPlugins_.erase(it);
    }

    if (loaded.instance && loaded.instance->state() >= core::PluginState::Initialized) {
        try {
            loaded.instance->shutdown();
        } catch (const std::exception& e) {
            spdlog::error("Error during plugin shutdown: {} - {}", pluginId, e.what());
        }
    }

    loaded.instance.reset();

    if (loaded.ownsHandle && loaded.handle) {
        closeLibrary(loaded.handle);
    }

    spdlog::info("Plugin unloaded: {}", pluginId);

    for (const auto& callback : unloadedCallbacks_) {
        callback(pluginId);
    }

    return true;
}

void PluginManager::unloadAllPlugins() {
    std::vector<std::string> pluginIds;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, _] : loadedPlugins_) {
            pluginIds.push_back(id);
        }
    }

    for (const auto& id : pluginIds) {
        unloadPlugin(id);
    }
}

bool PluginManager::initializePlugin(const std::string& pluginId) {
    std::shared_ptr<core::IPlugin> plugin;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loadedPlugins_.find(pluginId);
        if (it == loadedPlugins_.end()) {
            spdlog::error("Cannot initialize plugin, not loaded: {}", pluginId);
            return false;
        }
        plugin = it->second.instance;
    }

    if (plugin->state() >= core::PluginState::Initialized) {
        spdlog::debug("Plugin already initialized: {}", pluginId);
        return true;
    }

    try {
        if (!plugin->initialize(context_.get())) {
            spdlog::error("Plugin initialization failed: {}", pluginId);
            for (const auto& callback : errorCallbacks_) {
                callback(pluginId, "Initialization failed");
            }
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception during plugin initialization: {} - {}", pluginId, e.what());
        for (const auto& callback : errorCallbacks_) {
            callback(pluginId, std::string("Initialization exception: ") + e.what());
        }
        return false;
    }

    spdlog::info("Plugin initialized: {}", pluginId);
    return true;
}

void PluginManager::initializeAllPlugins() {
    std::vector<std::string> pluginIds;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, _] : loadedPlugins_) {
            pluginIds.push_back(id);
        }
    }

    for (const auto& id : pluginIds) {
        initializePlugin(id);
    }
}

void PluginManager::shutdownAllPlugins() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [id, loaded] : loadedPlugins_) {
        if (loaded.instance && loaded.instance->state() >= core::PluginState::Initialized) {
            try {
                loaded.instance->shutdown();
                spdlog::info("Plugin shutdown: {}", id);
            } catch (const std::exception& e) {
                spdlog::error("Error during plugin shutdown: {} - {}", id, e.what());
            }
        }
    }
}

void PluginManager::enablePlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end() && it->second.instance) {
        it->second.instance->enable();
        spdlog::info("Plugin enabled: {}", pluginId);
    }
}

void PluginManager::disablePlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end() && it->second.instance) {
        it->second.instance->disable();
        spdlog::info("Plugin disabled: {}", pluginId);
    }
}

std::vector<PluginInfo> PluginManager::discoverPlugins() const {
    std::vector<PluginInfo> plugins;

    if (!std::filesystem::exists(pluginDir_)) {
        return plugins;
    }

    for (const auto& entry : std::filesystem::directory_iterator(pluginDir_)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        auto ext = entry.path().extension().string();
#ifdef _WIN32
        if (ext != ".dll") continue;
#elif defined(__APPLE__)
        if (ext != ".dylib") continue;
#else
        if (ext != ".so") continue;
#endif

        auto manifestPath = entry.path();
        manifestPath.replace_extension(".json");

        if (std::filesystem::exists(manifestPath)) {
            try {
                std::ifstream file(manifestPath);
                nlohmann::json j;
                file >> j;

                PluginInfo info;
                info.metadata = core::PluginMetadata::fromJson(j);
                info.path = entry.path();
                info.enabled = j.value("enabled", true);
                info.configuration = j.value("configuration", nlohmann::json::object());

                plugins.push_back(std::move(info));
            } catch (const std::exception& e) {
                spdlog::warn("Failed to read plugin manifest: {} - {}", manifestPath.string(), e.what());
            }
        }
    }

    return plugins;
}

void PluginManager::refreshPluginList() {
    availablePlugins_ = discoverPlugins();
    spdlog::info("Discovered {} plugins", availablePlugins_.size());
}

std::shared_ptr<core::IPlugin> PluginManager::getPlugin(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end()) {
        return it->second.instance;
    }
    return nullptr;
}

std::vector<std::shared_ptr<core::IPlugin>> PluginManager::getPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<core::IPlugin>> result;
    result.reserve(loadedPlugins_.size());
    for (const auto& [_, loaded] : loadedPlugins_) {
        result.push_back(loaded.instance);
    }
    return result;
}

std::vector<std::shared_ptr<core::IPlugin>> PluginManager::getPluginsByType(core::PluginType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<core::IPlugin>> result;
    for (const auto& [_, loaded] : loadedPlugins_) {
        if (loaded.instance && loaded.instance->metadata().type == type) {
            result.push_back(loaded.instance);
        }
    }
    return result;
}

bool PluginManager::isLoaded(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadedPlugins_.find(pluginId) != loadedPlugins_.end();
}

core::PluginState PluginManager::getPluginState(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end() && it->second.instance) {
        return it->second.instance->state();
    }
    return core::PluginState::Unloaded;
}

std::vector<std::string> PluginManager::getLoadedPluginIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(loadedPlugins_.size());
    for (const auto& [id, _] : loadedPlugins_) {
        result.push_back(id);
    }
    return result;
}

std::vector<core::INetworkMonitorPlugin*> PluginManager::getNetworkMonitors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::INetworkMonitorPlugin*> result;
    for (const auto& [_, loaded] : loadedPlugins_) {
        if (auto* monitor = dynamic_cast<core::INetworkMonitorPlugin*>(loaded.instance.get())) {
            result.push_back(monitor);
        }
    }
    return result;
}

std::vector<core::INotificationPlugin*> PluginManager::getNotificationHandlers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::INotificationPlugin*> result;
    for (const auto& [_, loaded] : loadedPlugins_) {
        if (auto* handler = dynamic_cast<core::INotificationPlugin*>(loaded.instance.get())) {
            result.push_back(handler);
        }
    }
    return result;
}

std::vector<core::IDataProcessorPlugin*> PluginManager::getDataProcessors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::IDataProcessorPlugin*> result;
    for (const auto& [_, loaded] : loadedPlugins_) {
        if (auto* processor = dynamic_cast<core::IDataProcessorPlugin*>(loaded.instance.get())) {
            result.push_back(processor);
        }
    }
    return result;
}

std::vector<core::IDataExporterPlugin*> PluginManager::getDataExporters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::IDataExporterPlugin*> result;
    for (const auto& [_, loaded] : loadedPlugins_) {
        if (auto* exporter = dynamic_cast<core::IDataExporterPlugin*>(loaded.instance.get())) {
            result.push_back(exporter);
        }
    }
    return result;
}

void PluginManager::setPluginConfig(const std::string& pluginId, const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end() && it->second.instance) {
        it->second.instance->configure(config);
    }
}

nlohmann::json PluginManager::getPluginConfig(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end() && it->second.instance) {
        return it->second.instance->configuration();
    }
    return nlohmann::json::object();
}

bool PluginManager::savePluginStates(const std::filesystem::path& path) const {
    try {
        nlohmann::json j;
        j["plugins"] = nlohmann::json::array();

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, loaded] : loadedPlugins_) {
            if (!loaded.instance) continue;

            nlohmann::json pluginState;
            pluginState["id"] = id;
            pluginState["path"] = loaded.path.string();
            pluginState["enabled"] = loaded.instance->isEnabled();
            pluginState["configuration"] = loaded.instance->configuration();

            j["plugins"].push_back(pluginState);
        }

        std::ofstream file(path);
        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save plugin states: {}", e.what());
        return false;
    }
}

bool PluginManager::loadPluginStates(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return true;
    }

    try {
        std::ifstream file(path);
        nlohmann::json j;
        file >> j;

        if (!j.contains("plugins") || !j["plugins"].is_array()) {
            return true;
        }

        for (const auto& pluginState : j["plugins"]) {
            std::string pluginPath = pluginState.value("path", "");
            if (pluginPath.empty()) continue;

            if (!loadPlugin(pluginPath)) {
                continue;
            }

            std::string id = pluginState.value("id", "");
            if (id.empty()) continue;

            if (!initializePlugin(id)) {
                continue;
            }

            if (pluginState.contains("configuration")) {
                setPluginConfig(id, pluginState["configuration"]);
            }

            bool enabled = pluginState.value("enabled", true);
            if (!enabled) {
                disablePlugin(id);
            }
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load plugin states: {}", e.what());
        return false;
    }
}

void PluginManager::registerService(const std::string& serviceName, void* service) {
    context_->registerService(serviceName, service);
}

void PluginManager::unregisterService(const std::string& serviceName) {
    context_->unregisterService(serviceName);
}

int64_t PluginManager::subscribe(const std::string& eventName, core::IPluginContext::EventCallback callback) {
    return context_->subscribe(eventName, std::move(callback));
}

void PluginManager::unsubscribe(int64_t subscriptionId) {
    context_->unsubscribe(subscriptionId);
}

void PluginManager::publish(const std::string& eventName, const std::any& data) {
    context_->publish(eventName, data);
}

void PluginManager::onPluginLoaded(PluginLoadedCallback callback) {
    loadedCallbacks_.push_back(std::move(callback));
}

void PluginManager::onPluginUnloaded(PluginUnloadedCallback callback) {
    unloadedCallbacks_.push_back(std::move(callback));
}

void PluginManager::onPluginError(PluginErrorCallback callback) {
    errorCallbacks_.push_back(std::move(callback));
}

bool PluginManager::validateDependencies(const core::PluginMetadata& metadata) const {
    if (!isVersionCompatible(metadata.minAppVersion, appVersion_)) {
        spdlog::error("Plugin {} requires app version >= {}, current is {}",
            metadata.id, metadata.minAppVersion, appVersion_);
        return false;
    }

    for (const auto& dep : metadata.dependencies) {
        if (!dep.required) {
            continue;
        }

        auto plugin = getPlugin(dep.pluginName);
        if (!plugin) {
            spdlog::error("Plugin {} requires missing dependency: {}", metadata.id, dep.pluginName);
            return false;
        }

        if (!isVersionCompatible(dep.minVersion, plugin->metadata().version)) {
            spdlog::error("Plugin {} requires {} >= {}, found {}",
                metadata.id, dep.pluginName, dep.minVersion, plugin->metadata().version);
            return false;
        }
    }

    return true;
}

bool PluginManager::isVersionCompatible(const std::string& required, const std::string& actual) const {
    auto parseVersion = [](const std::string& v) -> std::tuple<int, int, int> {
        std::regex re(R"((\d+)\.(\d+)\.(\d+))");
        std::smatch match;
        if (std::regex_search(v, match, re)) {
            return {std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3])};
        }
        return {0, 0, 0};
    };

    auto [reqMajor, reqMinor, reqPatch] = parseVersion(required);
    auto [actMajor, actMinor, actPatch] = parseVersion(actual);

    if (actMajor > reqMajor) return true;
    if (actMajor < reqMajor) return false;

    if (actMinor > reqMinor) return true;
    if (actMinor < reqMinor) return false;

    return actPatch >= reqPatch;
}

void* PluginManager::openLibrary(const std::filesystem::path& path) {
#ifdef _WIN32
    return LoadLibraryA(path.string().c_str());
#else
    return dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void PluginManager::closeLibrary(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* PluginManager::getSymbol(void* handle, const std::string& name) {
    if (!handle) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
#else
    return dlsym(handle, name.c_str());
#endif
}

} // namespace netpulse::infra
