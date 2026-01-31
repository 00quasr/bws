#include "infrastructure/plugin/PluginContext.hpp"

#include <spdlog/spdlog.h>

namespace netpulse::infra {

PluginContext::PluginContext(
    const std::string& configDir,
    const std::string& dataDir,
    const std::string& pluginDir,
    const std::string& appVersion)
    : configDir_(configDir)
    , dataDir_(dataDir)
    , pluginDir_(pluginDir)
    , appVersion_(appVersion) {
}

void* PluginContext::getService(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        return it->second;
    }
    return nullptr;
}

void PluginContext::registerService(const std::string& serviceName, void* service) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    services_[serviceName] = service;
    spdlog::debug("Plugin service registered: {}", serviceName);
}

void PluginContext::unregisterService(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    services_.erase(serviceName);
    spdlog::debug("Plugin service unregistered: {}", serviceName);
}

bool PluginContext::hasService(const std::string& serviceName) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    return services_.find(serviceName) != services_.end();
}

int64_t PluginContext::subscribe(const std::string& eventName, EventCallback callback) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    int64_t id = nextSubscriptionId_++;
    subscriptions_.push_back({id, eventName, std::move(callback)});
    spdlog::debug("Plugin subscribed to event: {} (id={})", eventName, id);
    return id;
}

void PluginContext::unsubscribe(int64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
            [subscriptionId](const Subscription& sub) {
                return sub.id == subscriptionId;
            }),
        subscriptions_.end());
    spdlog::debug("Plugin unsubscribed (id={})", subscriptionId);
}

void PluginContext::publish(const std::string& eventName, const std::any& data) {
    std::vector<EventCallback> callbacks;

    {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        for (const auto& sub : subscriptions_) {
            if (sub.eventName == eventName) {
                callbacks.push_back(sub.callback);
            }
        }
    }

    for (const auto& callback : callbacks) {
        try {
            callback(eventName, data);
        } catch (const std::exception& e) {
            spdlog::error("Error in plugin event handler for '{}': {}", eventName, e.what());
        }
    }
}

void PluginContext::log(const std::string& level, const std::string& message) {
    if (level == "debug") {
        spdlog::debug("[plugin] {}", message);
    } else if (level == "info") {
        spdlog::info("[plugin] {}", message);
    } else if (level == "warning" || level == "warn") {
        spdlog::warn("[plugin] {}", message);
    } else if (level == "error") {
        spdlog::error("[plugin] {}", message);
    } else {
        spdlog::info("[plugin] {}", message);
    }
}

} // namespace netpulse::infra
