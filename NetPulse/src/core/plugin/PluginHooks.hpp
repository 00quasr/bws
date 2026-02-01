/**
 * @file PluginHooks.hpp
 * @brief Specialized plugin interfaces for different plugin types.
 *
 * This file defines the specific interfaces that plugins must implement
 * based on their type (network monitor, notification, data processor, exporter).
 */

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

/**
 * @brief Result from a network monitor plugin check.
 */
struct NetworkMonitorResult {
    std::string monitorType;    ///< Type of monitor that produced this result
    std::string targetAddress;  ///< Address that was monitored
    bool success{false};        ///< Whether the check succeeded
    std::chrono::milliseconds latency{0}; ///< Response latency
    std::string errorMessage;   ///< Error message if check failed
    nlohmann::json additionalData; ///< Additional monitor-specific data
    std::chrono::system_clock::time_point timestamp; ///< When the check was performed
};

/**
 * @brief Interface for custom network monitoring plugins.
 *
 * Allows plugins to implement custom monitoring protocols beyond
 * the built-in ICMP ping and SNMP support.
 */
class INetworkMonitorPlugin {
public:
    virtual ~INetworkMonitorPlugin() = default;

    /**
     * @brief Gets the type identifier for this monitor.
     * @return String identifying the monitor type (e.g., "http", "tcp", "dns").
     */
    [[nodiscard]] virtual std::string monitorType() const = 0;

    /**
     * @brief Checks if this monitor supports the given address format.
     * @param address The address to check.
     * @return True if the monitor can handle this address format.
     */
    [[nodiscard]] virtual bool supportsAddress(const std::string& address) const = 0;

    /**
     * @brief Callback function type for monitor results.
     * @param result The monitoring result.
     */
    using MonitorCallback = std::function<void(const NetworkMonitorResult&)>;

    /**
     * @brief Starts periodic monitoring of a host.
     * @param host The host to monitor.
     * @param callback Function to call with each monitoring result.
     */
    virtual void startMonitoring(const Host& host, MonitorCallback callback) = 0;

    /**
     * @brief Stops monitoring a specific host.
     * @param hostId ID of the host to stop monitoring.
     */
    virtual void stopMonitoring(int64_t hostId) = 0;

    /**
     * @brief Stops monitoring all hosts.
     */
    virtual void stopAllMonitoring() = 0;

    /**
     * @brief Checks if a host is currently being monitored.
     * @param hostId ID of the host to check.
     * @return True if the host is being monitored.
     */
    [[nodiscard]] virtual bool isMonitoring(int64_t hostId) const = 0;

    /**
     * @brief Performs an asynchronous check of the target.
     * @param address Target address to check.
     * @param timeout Maximum time to wait for response.
     * @return Future containing the monitor result.
     */
    virtual std::future<NetworkMonitorResult> checkAsync(
        const std::string& address,
        std::chrono::milliseconds timeout) = 0;
};

/**
 * @brief Payload for notification delivery.
 */
struct NotificationPayload {
    std::string title;        ///< Notification title
    std::string message;      ///< Notification message body
    std::string severity;     ///< Severity level (e.g., "info", "warning", "critical")
    nlohmann::json metadata;  ///< Additional metadata
    std::chrono::system_clock::time_point timestamp; ///< When the notification was created
};

/**
 * @brief Result of a notification delivery attempt from a plugin.
 */
struct PluginNotificationResult {
    bool success{false};      ///< Whether delivery succeeded
    std::string errorMessage; ///< Error message if delivery failed
    int httpStatusCode{0};    ///< HTTP status code from the endpoint
    std::chrono::milliseconds deliveryTime{0}; ///< Time taken to deliver
};

/**
 * @brief Interface for custom notification handler plugins.
 *
 * Allows plugins to implement delivery to notification services beyond
 * the built-in Slack, Discord, and PagerDuty support.
 */
class INotificationPlugin {
public:
    virtual ~INotificationPlugin() = default;

    /**
     * @brief Gets the type identifier for this notification handler.
     * @return String identifying the notification type (e.g., "email", "sms", "teams").
     */
    [[nodiscard]] virtual std::string notificationType() const = 0;

    /**
     * @brief Gets the JSON schema for endpoint configuration.
     * @return JSON schema describing required configuration fields.
     */
    [[nodiscard]] virtual nlohmann::json configurationSchema() const = 0;

    /**
     * @brief Sends a notification to the configured endpoint.
     * @param payload The notification content to send.
     * @param endpointConfig Endpoint-specific configuration.
     * @return Result of the delivery attempt.
     */
    virtual PluginNotificationResult send(
        const NotificationPayload& payload,
        const nlohmann::json& endpointConfig) = 0;

    /**
     * @brief Tests connectivity to the endpoint.
     * @param endpointConfig Endpoint-specific configuration.
     * @return Result indicating success/failure of connection test.
     */
    virtual PluginNotificationResult testConnection(const nlohmann::json& endpointConfig) = 0;

    /**
     * @brief Formats the payload for this notification type.
     * @param payload The notification content.
     * @param endpointConfig Endpoint-specific configuration.
     * @return Formatted payload string (e.g., JSON, XML).
     */
    [[nodiscard]] virtual std::string formatPayload(
        const NotificationPayload& payload,
        const nlohmann::json& endpointConfig) const = 0;
};

/**
 * @brief Result of data processing by a plugin.
 */
struct ProcessedData {
    std::string dataType;            ///< Type of data that was processed
    nlohmann::json originalData;     ///< Original unprocessed data
    nlohmann::json enrichedData;     ///< Enriched/processed data
    std::vector<std::string> tags;   ///< Tags added during processing
    std::chrono::system_clock::time_point processedAt; ///< When processing occurred
};

/**
 * @brief Interface for data processing plugins.
 *
 * Allows plugins to enrich, correlate, or transform monitoring data
 * before it is stored or displayed.
 */
class IDataProcessorPlugin {
public:
    virtual ~IDataProcessorPlugin() = default;

    /**
     * @brief Gets the list of data types this processor can handle.
     * @return Vector of supported data type identifiers.
     */
    [[nodiscard]] virtual std::vector<std::string> supportedDataTypes() const = 0;

    /**
     * @brief Processes data of the specified type.
     * @param dataType Type of the data being processed.
     * @param data The data to process.
     * @return Processed data with enrichments.
     */
    virtual ProcessedData process(const std::string& dataType, const nlohmann::json& data) = 0;

    /**
     * @brief Checks if this processor can handle the specified data type.
     * @param dataType The data type to check.
     * @return True if the processor can handle this type.
     */
    [[nodiscard]] virtual bool canProcess(const std::string& dataType) const = 0;

    /**
     * @brief Event hook called when a ping result is received.
     * @param result The ping result.
     */
    virtual void onPingResult(const PingResult& /*result*/) {}

    /**
     * @brief Event hook called when an alert is generated.
     * @param alert The generated alert.
     */
    virtual void onAlert(const Alert& /*alert*/) {}

    /**
     * @brief Event hook called when a port scan completes.
     * @param result The port scan result.
     */
    virtual void onPortScanComplete(const PortScanResult& /*result*/) {}
};

/**
 * @brief Result of a data export operation.
 */
struct ExportResult {
    bool success{false};       ///< Whether export succeeded
    std::string errorMessage;  ///< Error message if export failed
    int64_t recordsExported{0}; ///< Number of records exported
    std::chrono::milliseconds duration{0}; ///< Time taken for export
};

/**
 * @brief Interface for data exporter plugins.
 *
 * Allows plugins to export monitoring data to external systems
 * or file formats.
 */
class IDataExporterPlugin {
public:
    virtual ~IDataExporterPlugin() = default;

    /**
     * @brief Gets the type identifier for this exporter.
     * @return String identifying the exporter type (e.g., "csv", "influxdb", "prometheus").
     */
    [[nodiscard]] virtual std::string exporterType() const = 0;

    /**
     * @brief Gets the list of supported export formats.
     * @return Vector of format identifiers this exporter supports.
     */
    [[nodiscard]] virtual std::vector<std::string> supportedFormats() const = 0;

    /**
     * @brief Exports data to an external system.
     * @param format The format to export in.
     * @param data The data to export.
     * @param options Export options (connection details, etc.).
     * @return Result of the export operation.
     */
    virtual ExportResult exportData(
        const std::string& format,
        const nlohmann::json& data,
        const nlohmann::json& options) = 0;

    /**
     * @brief Exports data to a file.
     * @param format The format to export in.
     * @param data The data to export.
     * @param filePath Path to the output file.
     * @return Result of the export operation.
     */
    virtual ExportResult exportToFile(
        const std::string& format,
        const nlohmann::json& data,
        const std::string& filePath) = 0;

    /**
     * @brief Tests connectivity to the export destination.
     * @param options Connection options to test.
     * @return True if connection test succeeded.
     */
    [[nodiscard]] virtual bool testConnection(const nlohmann::json& options) = 0;
};

} // namespace netpulse::core
