#pragma once

#include "core/types/Alert.hpp"
#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "core/types/PortScanResult.hpp"

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::core {

struct NetworkMonitorResult {
    std::string monitorType;
    std::string targetAddress;
    bool success{false};
    std::chrono::milliseconds latency{0};
    std::string errorMessage;
    nlohmann::json additionalData;
    std::chrono::system_clock::time_point timestamp;
};

class INetworkMonitorPlugin {
public:
    virtual ~INetworkMonitorPlugin() = default;

    [[nodiscard]] virtual std::string monitorType() const = 0;

    [[nodiscard]] virtual bool supportsAddress(const std::string& address) const = 0;

    using MonitorCallback = std::function<void(const NetworkMonitorResult&)>;
    virtual void startMonitoring(const Host& host, MonitorCallback callback) = 0;
    virtual void stopMonitoring(int64_t hostId) = 0;
    virtual void stopAllMonitoring() = 0;

    [[nodiscard]] virtual bool isMonitoring(int64_t hostId) const = 0;

    virtual std::future<NetworkMonitorResult> checkAsync(
        const std::string& address,
        std::chrono::milliseconds timeout) = 0;
};

struct NotificationPayload {
    std::string title;
    std::string message;
    std::string severity;
    nlohmann::json metadata;
    std::chrono::system_clock::time_point timestamp;
};

struct PluginNotificationResult {
    bool success{false};
    std::string errorMessage;
    int httpStatusCode{0};
    std::chrono::milliseconds deliveryTime{0};
};

class INotificationPlugin {
public:
    virtual ~INotificationPlugin() = default;

    [[nodiscard]] virtual std::string notificationType() const = 0;

    [[nodiscard]] virtual nlohmann::json configurationSchema() const = 0;

    virtual PluginNotificationResult send(
        const NotificationPayload& payload,
        const nlohmann::json& endpointConfig) = 0;

    virtual PluginNotificationResult testConnection(const nlohmann::json& endpointConfig) = 0;

    [[nodiscard]] virtual std::string formatPayload(
        const NotificationPayload& payload,
        const nlohmann::json& endpointConfig) const = 0;
};

struct ProcessedData {
    std::string dataType;
    nlohmann::json originalData;
    nlohmann::json enrichedData;
    std::vector<std::string> tags;
    std::chrono::system_clock::time_point processedAt;
};

class IDataProcessorPlugin {
public:
    virtual ~IDataProcessorPlugin() = default;

    [[nodiscard]] virtual std::vector<std::string> supportedDataTypes() const = 0;

    virtual ProcessedData process(const std::string& dataType, const nlohmann::json& data) = 0;

    [[nodiscard]] virtual bool canProcess(const std::string& dataType) const = 0;

    virtual void onPingResult(const PingResult& /*result*/) {}
    virtual void onAlert(const Alert& /*alert*/) {}
    virtual void onPortScanComplete(const PortScanResult& /*result*/) {}
};

struct ExportResult {
    bool success{false};
    std::string errorMessage;
    int64_t recordsExported{0};
    std::chrono::milliseconds duration{0};
};

class IDataExporterPlugin {
public:
    virtual ~IDataExporterPlugin() = default;

    [[nodiscard]] virtual std::string exporterType() const = 0;

    [[nodiscard]] virtual std::vector<std::string> supportedFormats() const = 0;

    virtual ExportResult exportData(
        const std::string& format,
        const nlohmann::json& data,
        const nlohmann::json& options) = 0;

    virtual ExportResult exportToFile(
        const std::string& format,
        const nlohmann::json& data,
        const std::string& filePath) = 0;

    [[nodiscard]] virtual bool testConnection(const nlohmann::json& options) = 0;
};

} // namespace netpulse::core
