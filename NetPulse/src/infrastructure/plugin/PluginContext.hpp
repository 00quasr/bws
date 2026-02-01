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

/**
 * @brief Context object provided to plugins for accessing application services.
 *
 * Implements core::IPluginContext to provide plugins with access to
 * application services, event pub/sub, logging, and directory paths.
 * Thread-safe for concurrent access.
 */
class PluginContext : public core::IPluginContext {
public:
    /**
     * @brief Constructs a PluginContext with application paths.
     * @param configDir Path to the configuration directory.
     * @param dataDir Path to the data directory.
     * @param pluginDir Path to the plugins directory.
     * @param appVersion Application version string.
     */
    PluginContext(
        const std::string& configDir,
        const std::string& dataDir,
        const std::string& pluginDir,
        const std::string& appVersion);

    /**
     * @brief Default destructor.
     */
    ~PluginContext() override = default;

    /**
     * @brief Retrieves a registered service by name.
     * @param serviceName Name of the service to retrieve.
     * @return Pointer to the service, or nullptr if not found.
     */
    void* getService(const std::string& serviceName) override;

    /**
     * @brief Registers a service for plugin access.
     * @param serviceName Name to register the service under.
     * @param service Pointer to the service instance.
     */
    void registerService(const std::string& serviceName, void* service) override;

    /**
     * @brief Unregisters a service.
     * @param serviceName Name of the service to unregister.
     */
    void unregisterService(const std::string& serviceName) override;

    /**
     * @brief Checks if a service is registered.
     * @param serviceName Name of the service.
     * @return True if registered, false otherwise.
     */
    [[nodiscard]] bool hasService(const std::string& serviceName) const override;

    /**
     * @brief Subscribes to an event.
     * @param eventName Name of the event to subscribe to.
     * @param callback Function called when the event is published.
     * @return Subscription ID for later unsubscription.
     */
    int64_t subscribe(const std::string& eventName, EventCallback callback) override;

    /**
     * @brief Unsubscribes from an event.
     * @param subscriptionId ID returned from subscribe().
     */
    void unsubscribe(int64_t subscriptionId) override;

    /**
     * @brief Publishes an event to all subscribers.
     * @param eventName Name of the event.
     * @param data Event data payload.
     */
    void publish(const std::string& eventName, const std::any& data) override;

    /**
     * @brief Returns the configuration directory path.
     * @return Configuration directory path.
     */
    [[nodiscard]] std::string configDir() const override { return configDir_; }

    /**
     * @brief Returns the data directory path.
     * @return Data directory path.
     */
    [[nodiscard]] std::string dataDir() const override { return dataDir_; }

    /**
     * @brief Returns the plugins directory path.
     * @return Plugins directory path.
     */
    [[nodiscard]] std::string pluginDir() const override { return pluginDir_; }

    /**
     * @brief Returns the application version string.
     * @return Application version.
     */
    [[nodiscard]] std::string appVersion() const override { return appVersion_; }

    /**
     * @brief Logs a message from a plugin.
     * @param level Log level (e.g., "info", "warn", "error").
     * @param message Log message.
     */
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
