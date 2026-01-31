#pragma once

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>

namespace netpulse::core {

class IPlugin;

class IPluginContext {
public:
    virtual ~IPluginContext() = default;

    virtual void* getService(const std::string& serviceName) = 0;

    template<typename T>
    T* getService(const std::string& serviceName) {
        return static_cast<T*>(getService(serviceName));
    }

    virtual void registerService(const std::string& serviceName, void* service) = 0;
    virtual void unregisterService(const std::string& serviceName) = 0;
    [[nodiscard]] virtual bool hasService(const std::string& serviceName) const = 0;

    using EventCallback = std::function<void(const std::string&, const std::any&)>;
    virtual int64_t subscribe(const std::string& eventName, EventCallback callback) = 0;
    virtual void unsubscribe(int64_t subscriptionId) = 0;
    virtual void publish(const std::string& eventName, const std::any& data) = 0;

    [[nodiscard]] virtual std::string configDir() const = 0;
    [[nodiscard]] virtual std::string dataDir() const = 0;
    [[nodiscard]] virtual std::string pluginDir() const = 0;

    [[nodiscard]] virtual std::string appVersion() const = 0;

    virtual void log(const std::string& level, const std::string& message) = 0;

    void logDebug(const std::string& message) { log("debug", message); }
    void logInfo(const std::string& message) { log("info", message); }
    void logWarning(const std::string& message) { log("warning", message); }
    void logError(const std::string& message) { log("error", message); }
};

} // namespace netpulse::core
