/**
 * @file Notification.hpp
 * @brief Webhook notification types and configuration structures.
 *
 * This file defines the webhook configuration, delivery logging, and
 * notification status types used by the notification service.
 */

#pragma once

#include "core/types/Alert.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::core {

/**
 * @brief Supported webhook provider types.
 */
enum class WebhookProvider : int {
    Slack = 0,    ///< Slack incoming webhook
    Discord = 1,  ///< Discord webhook
    PagerDuty = 2 ///< PagerDuty Events API
};

/**
 * @brief Configuration for a webhook endpoint.
 *
 * Defines how alerts should be sent to an external notification service.
 */
struct WebhookConfig {
    int64_t id{0};                              ///< Unique identifier for the webhook
    std::string name;                           ///< Human-readable name for the webhook
    WebhookProvider provider{WebhookProvider::Slack}; ///< Type of webhook provider
    std::string url;                            ///< Webhook URL endpoint
    std::string apiToken;                       ///< Optional API token for authentication
    bool enabled{true};                         ///< Whether the webhook is active
    std::vector<AlertSeverity> severityFilter;  ///< Only send alerts matching these severities
    std::vector<AlertType> typeFilter;          ///< Only send alerts matching these types
    int timeoutMs{5000};                        ///< HTTP request timeout in milliseconds
    int maxRetries{3};                          ///< Maximum retry attempts on failure

    /**
     * @brief Converts the webhook provider to a string.
     * @return String representation of the provider (e.g., "Slack", "Discord").
     */
    [[nodiscard]] std::string providerToString() const;

    /**
     * @brief Parses a string to get the corresponding WebhookProvider.
     * @param str The string to parse (e.g., "Slack", "Discord", "PagerDuty").
     * @return The corresponding WebhookProvider enum value.
     */
    static WebhookProvider providerFromString(const std::string& str);

    /**
     * @brief Checks if an alert matches this webhook's filters.
     * @param alert The alert to check against the filters.
     * @return True if the alert should be sent to this webhook.
     */
    [[nodiscard]] bool matchesAlert(const Alert& alert) const;

    bool operator==(const WebhookConfig& other) const = default;
};

/**
 * @brief Log entry for a webhook delivery attempt.
 *
 * Records the outcome of attempting to send an alert to a webhook.
 */
struct WebhookDeliveryLog {
    int64_t id{0};          ///< Unique identifier for the log entry
    int64_t webhookId{0};   ///< ID of the webhook that was used
    int64_t alertId{0};     ///< ID of the alert that was sent
    bool success{false};    ///< Whether the delivery was successful
    int httpStatus{0};      ///< HTTP status code from the response
    std::string errorMessage; ///< Error message if delivery failed
    std::chrono::system_clock::time_point sentAt; ///< When the delivery was attempted
};

/**
 * @brief Result of a notification delivery attempt.
 */
enum class NotificationResult {
    Success,  ///< Notification was delivered successfully
    Failed,   ///< Notification delivery failed
    Retrying, ///< Delivery is being retried
    Skipped   ///< Notification was skipped (e.g., filtered out)
};

/**
 * @brief Status of a notification delivery.
 *
 * Provides details about a notification delivery attempt including
 * success/failure status and any error information.
 */
struct NotificationStatus {
    NotificationResult result{NotificationResult::Failed}; ///< Delivery result
    std::string webhookName;  ///< Name of the webhook used
    std::string errorMessage; ///< Error message if delivery failed
    int httpStatus{0};        ///< HTTP status code from the response
    int retryCount{0};        ///< Number of retry attempts made
};

} // namespace netpulse::core
