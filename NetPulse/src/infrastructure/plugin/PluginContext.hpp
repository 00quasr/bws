#pragma once

#include "core/plugin/IPluginContext.hpp"

#include <any>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace netpulse::infra {

class PluginContext : public core::IPluginContext {
public:
    PluginContext(
        const std::string& configDir,
        const std::string& dataDir,
        const std::string& pluginDir,
        const std::string& appVersion);

    ~PluginContext() override = default;

    void* getService(const std::string& serviceName) override;
    void registerService(const std::string& serviceName, void* service) override;
    void unregisterService(const std::string& serviceName) override;
    [[nodiscard]] bool hasService(const std::string& serviceName) const override;

    int64_t subscribe(const std::string& eventName, EventCallback callback) override;
    void unsubscribe(int64_t subscriptionId) override;
    void publish(const std::string& eventName, const std::any& data) override;

    [[nodiscard]] std::string configDir() const override { return configDir_; }
    [[nodiscard]] std::string dataDir() const override { return dataDir_; }
    [[nodiscard]] std::string pluginDir() const override { return pluginDir_; }
    [[nodiscard]] std::string appVersion() const override { return appVersion_; }

    void log(const std::string& level, const std::string& message) override;

private:
    struct Subscription {
        int64_t id;
        std::string eventName;
        EventCallback callback;
    };

    std::string configDir_;
    std::string dataDir_;
    std::string pluginDir_;
    std::string appVersion_;

    mutable std::mutex servicesMutex_;
    std::map<std::string, void*> services_;

    mutable std::mutex subscriptionsMutex_;
    std::vector<Subscription> subscriptions_;
    std::atomic<int64_t> nextSubscriptionId_{1};
};

} // namespace netpulse::infra
