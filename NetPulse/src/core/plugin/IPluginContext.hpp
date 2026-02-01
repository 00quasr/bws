/**
 * @file IPluginContext.hpp
 * @brief Plugin context interface for accessing application services.
 *
 * This file defines the context interface that is provided to plugins,
 * allowing them to access services, publish events, and log messages.
 */

#pragma once

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>

namespace netpulse::core {

class IPlugin;

/**
 * @brief Context interface provided to plugins for accessing application services.
 *
 * The plugin context provides access to registered services, event pub/sub,
 * directory paths, and logging facilities.
 */
class IPluginContext {
public:
    virtual ~IPluginContext() = default;

    /**
     * @brief Gets a service by name (untyped).
     * @param serviceName Name of the service to retrieve.
     * @return Pointer to the service, or nullptr if not found.
     */
    virtual void* getService(const std::string& serviceName) = 0;

    /**
     * @brief Gets a service by name with type safety.
     * @tparam T The expected service type.
     * @param serviceName Name of the service to retrieve.
     * @return Typed pointer to the service, or nullptr if not found.
     */
    template<typename T>
    T* getService(const std::string& serviceName) {
        return static_cast<T*>(getService(serviceName));
    }

    /**
     * @brief Registers a service with the context.
     * @param serviceName Name to register the service under.
     * @param service Pointer to the service instance.
     */
    virtual void registerService(const std::string& serviceName, void* service) = 0;

    /**
     * @brief Unregisters a service from the context.
     * @param serviceName Name of the service to unregister.
     */
    virtual void unregisterService(const std::string& serviceName) = 0;

    /**
     * @brief Checks if a service is registered.
     * @param serviceName Name of the service to check.
     * @return True if the service is registered.
     */
    [[nodiscard]] virtual bool hasService(const std::string& serviceName) const = 0;

    /**
     * @brief Callback function type for event subscriptions.
     * @param eventName Name of the event.
     * @param data Event data payload.
     */
    using EventCallback = std::function<void(const std::string&, const std::any&)>;

    /**
     * @brief Subscribes to an event.
     * @param eventName Name of the event to subscribe to.
     * @param callback Function to call when the event is published.
     * @return Subscription ID for later unsubscription.
     */
    virtual int64_t subscribe(const std::string& eventName, EventCallback callback) = 0;

    /**
     * @brief Unsubscribes from an event.
     * @param subscriptionId The subscription ID returned from subscribe().
     */
    virtual void unsubscribe(int64_t subscriptionId) = 0;

    /**
     * @brief Publishes an event to all subscribers.
     * @param eventName Name of the event to publish.
     * @param data Event data payload.
     */
    virtual void publish(const std::string& eventName, const std::any& data) = 0;

    /**
     * @brief Gets the configuration directory path.
     * @return Path to the configuration directory.
     */
    [[nodiscard]] virtual std::string configDir() const = 0;

    /**
     * @brief Gets the data directory path.
     * @return Path to the data directory.
     */
    [[nodiscard]] virtual std::string dataDir() const = 0;

    /**
     * @brief Gets the plugins directory path.
     * @return Path to the plugins directory.
     */
    [[nodiscard]] virtual std::string pluginDir() const = 0;

    /**
     * @brief Gets the application version.
     * @return Application version string.
     */
    [[nodiscard]] virtual std::string appVersion() const = 0;

    /**
     * @brief Logs a message at the specified level.
     * @param level Log level (e.g., "debug", "info", "warning", "error").
     * @param message The message to log.
     */
    virtual void log(const std::string& level, const std::string& message) = 0;

    /**
     * @brief Logs a debug message.
     * @param message The message to log.
     */
    void logDebug(const std::string& message) { log("debug", message); }

    /**
     * @brief Logs an informational message.
     * @param message The message to log.
     */
    void logInfo(const std::string& message) { log("info", message); }

    /**
     * @brief Logs a warning message.
     * @param message The message to log.
     */
    void logWarning(const std::string& message) { log("warning", message); }

    /**
     * @brief Logs an error message.
     * @param message The message to log.
     */
    void logError(const std::string& message) { log("error", message); }
};

} // namespace netpulse::core
