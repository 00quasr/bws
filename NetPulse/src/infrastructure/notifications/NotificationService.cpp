#include "infrastructure/notifications/NotificationService.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <sstream>

namespace netpulse::infra {

using json = nlohmann::json;

NotificationService::NotificationService(std::shared_ptr<Database> db, QObject* parent)
    : QObject(parent), db_(std::move(db)) {
    httpClient_ = std::make_unique<HttpClient>(this);
}

void NotificationService::sendAlert(const core::Alert& alert, const std::string& hostName) {
    std::lock_guard lock(mutex_);

    if (!enabled_) {
        spdlog::debug("Notifications disabled, skipping alert");
        return;
    }

    for (const auto& webhook : webhooks_) {
        if (webhook.matchesAlert(alert)) {
            sendToWebhook(webhook, alert, hostName);
        }
    }
}

void NotificationService::sendToWebhook(const core::WebhookConfig& webhook, const core::Alert& alert,
                                         const std::string& hostName, int retryCount) {
    std::string payload = buildPayload(webhook, alert, hostName);
    auto headers = getHeaders(webhook);

    spdlog::info("Sending alert to webhook: {} ({})", webhook.name, webhook.providerToString());

    httpClient_->postAsync(
        webhook.url, payload, headers, webhook.timeoutMs,
        [this, webhook, alert, hostName, retryCount](const HttpResponse& response) {
            core::NotificationStatus status;
            status.webhookName = webhook.name;
            status.httpStatus = response.statusCode;
            status.retryCount = retryCount;

            if (response.success) {
                status.result = core::NotificationResult::Success;
                spdlog::info("Webhook delivered successfully: {} (status: {})", webhook.name,
                             response.statusCode);
            } else {
                status.errorMessage = response.errorMessage;

                if (retryCount < webhook.maxRetries) {
                    status.result = core::NotificationResult::Retrying;
                    spdlog::warn("Webhook delivery failed, retrying ({}/{}): {} - {}", retryCount + 1,
                                 webhook.maxRetries, webhook.name, response.errorMessage);

                    int delayMs = 1000 * (1 << retryCount);
                    QTimer::singleShot(delayMs, this, [this, webhook, alert, hostName, retryCount]() {
                        sendToWebhook(webhook, alert, hostName, retryCount + 1);
                    });
                } else {
                    status.result = core::NotificationResult::Failed;
                    spdlog::error("Webhook delivery failed after {} retries: {} - {}",
                                  webhook.maxRetries, webhook.name, response.errorMessage);
                    emit webhookFailed(webhook, response.errorMessage);
                }
            }

            logDelivery(webhook.id, alert.id, response.success, response.statusCode,
                        response.errorMessage);

            for (const auto& callback : subscribers_) {
                callback(webhook, status);
            }

            if (response.success) {
                emit webhookDelivered(webhook, status);
            }
        });
}

void NotificationService::logDelivery(int64_t webhookId, int64_t alertId, bool success,
                                       int httpStatus, const std::string& errorMessage) {
    try {
        db_->execute(R"(
            INSERT INTO webhook_delivery_logs (webhook_id, alert_id, success, http_status, error_message, sent_at)
            VALUES (?, ?, ?, ?, ?, datetime('now'))
        )",
                     webhookId, alertId, success ? 1 : 0, httpStatus, errorMessage);
    } catch (const std::exception& e) {
        spdlog::error("Failed to log webhook delivery: {}", e.what());
    }
}

std::string NotificationService::buildPayload(const core::WebhookConfig& webhook,
                                                const core::Alert& alert,
                                                const std::string& hostName) const {
    switch (webhook.provider) {
    case core::WebhookProvider::Slack:
        return buildSlackPayload(alert, hostName);
    case core::WebhookProvider::Discord:
        return buildDiscordPayload(alert, hostName);
    case core::WebhookProvider::PagerDuty:
        return buildPagerDutyPayload(webhook, alert, hostName);
    default:
        return buildSlackPayload(alert, hostName);
    }
}

std::string NotificationService::buildSlackPayload(const core::Alert& alert,
                                                    const std::string& hostName) const {
    json payload;
    payload["text"] = "NetPulse Alert: " + alert.title;

    json blocks = json::array();

    json header;
    header["type"] = "header";
    header["text"]["type"] = "plain_text";
    header["text"]["text"] = alert.title;
    header["text"]["emoji"] = true;
    blocks.push_back(header);

    json fields;
    fields["type"] = "section";
    fields["fields"] = json::array();

    json severityField;
    severityField["type"] = "mrkdwn";
    severityField["text"] = "*Severity:*\n" + alert.severityToString();
    fields["fields"].push_back(severityField);

    json hostField;
    hostField["type"] = "mrkdwn";
    hostField["text"] = "*Host:*\n" + hostName;
    fields["fields"].push_back(hostField);

    json typeField;
    typeField["type"] = "mrkdwn";
    typeField["text"] = "*Type:*\n" + alert.typeToString();
    fields["fields"].push_back(typeField);

    json timeField;
    timeField["type"] = "mrkdwn";
    timeField["text"] = "*Time:*\n" + formatTimestamp(alert.timestamp);
    fields["fields"].push_back(timeField);

    blocks.push_back(fields);

    json messageSection;
    messageSection["type"] = "section";
    messageSection["text"]["type"] = "mrkdwn";
    messageSection["text"]["text"] = "*Message:*\n" + alert.message;
    blocks.push_back(messageSection);

    json divider;
    divider["type"] = "divider";
    blocks.push_back(divider);

    json context;
    context["type"] = "context";
    context["elements"] = json::array();
    json contextText;
    contextText["type"] = "mrkdwn";
    contextText["text"] = "Sent by NetPulse Network Monitor";
    context["elements"].push_back(contextText);
    blocks.push_back(context);

    payload["blocks"] = blocks;

    return payload.dump();
}

std::string NotificationService::buildDiscordPayload(const core::Alert& alert,
                                                      const std::string& hostName) const {
    json payload;
    payload["username"] = "NetPulse";
    payload["content"] = "";

    json embed;
    embed["title"] = alert.title;
    embed["color"] = severityToColor(alert.severity);
    embed["timestamp"] = formatTimestamp(alert.timestamp);

    json fields = json::array();

    json severityField;
    severityField["name"] = "Severity";
    severityField["value"] = alert.severityToString();
    severityField["inline"] = true;
    fields.push_back(severityField);

    json hostField;
    hostField["name"] = "Host";
    hostField["value"] = hostName;
    hostField["inline"] = true;
    fields.push_back(hostField);

    json typeField;
    typeField["name"] = "Type";
    typeField["value"] = alert.typeToString();
    typeField["inline"] = true;
    fields.push_back(typeField);

    json messageField;
    messageField["name"] = "Message";
    messageField["value"] = alert.message;
    messageField["inline"] = false;
    fields.push_back(messageField);

    embed["fields"] = fields;

    json footer;
    footer["text"] = "NetPulse Network Monitor";
    embed["footer"] = footer;

    payload["embeds"] = json::array({embed});

    return payload.dump();
}

std::string NotificationService::buildPagerDutyPayload(const core::WebhookConfig& webhook,
                                                        const core::Alert& alert,
                                                        const std::string& hostName) const {
    json payload;
    payload["routing_key"] = webhook.apiToken;

    if (alert.type == core::AlertType::HostRecovered) {
        payload["event_action"] = "resolve";
    } else {
        payload["event_action"] = "trigger";
    }

    payload["dedup_key"] = "netpulse-host-" + std::to_string(alert.hostId) + "-" + alert.typeToString();

    json payloadDetails;
    payloadDetails["summary"] = alert.title + ": " + hostName;
    payloadDetails["severity"] = severityToPagerDuty(alert.severity);
    payloadDetails["source"] = "NetPulse";
    payloadDetails["timestamp"] = formatTimestamp(alert.timestamp);

    json customDetails;
    customDetails["host"] = hostName;
    customDetails["alert_type"] = alert.typeToString();
    customDetails["message"] = alert.message;
    customDetails["host_id"] = alert.hostId;
    payloadDetails["custom_details"] = customDetails;

    payload["payload"] = payloadDetails;

    return payload.dump();
}

std::map<std::string, std::string> NotificationService::getHeaders(
    const core::WebhookConfig& webhook) const {
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";

    if (webhook.provider == core::WebhookProvider::PagerDuty) {
        headers["Content-Type"] = "application/json";
    }

    return headers;
}

std::string NotificationService::formatTimestamp(std::chrono::system_clock::time_point tp) const {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

int NotificationService::severityToColor(core::AlertSeverity severity) const {
    switch (severity) {
    case core::AlertSeverity::Critical:
        return 15158332;
    case core::AlertSeverity::Warning:
        return 16776960;
    case core::AlertSeverity::Info:
        return 3066993;
    default:
        return 9807270;
    }
}

std::string NotificationService::severityToPagerDuty(core::AlertSeverity severity) const {
    switch (severity) {
    case core::AlertSeverity::Critical:
        return "critical";
    case core::AlertSeverity::Warning:
        return "warning";
    case core::AlertSeverity::Info:
        return "info";
    default:
        return "info";
    }
}

void NotificationService::addWebhook(const core::WebhookConfig& config) {
    std::lock_guard lock(mutex_);

    std::string severityJson = "[]";
    std::string typeJson = "[]";

    if (!config.severityFilter.empty()) {
        json arr = json::array();
        for (auto s : config.severityFilter) {
            core::Alert temp;
            temp.severity = s;
            arr.push_back(temp.severityToString());
        }
        severityJson = arr.dump();
    }

    if (!config.typeFilter.empty()) {
        json arr = json::array();
        for (auto t : config.typeFilter) {
            core::Alert temp;
            temp.type = t;
            arr.push_back(temp.typeToString());
        }
        typeJson = arr.dump();
    }

    db_->execute(R"(
        INSERT INTO webhook_configs (name, provider, url, api_token, enabled, severity_filter, type_filter, timeout_ms, max_retries, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'), datetime('now'))
    )",
                 config.name, config.providerToString(), config.url, config.apiToken,
                 config.enabled ? 1 : 0, severityJson, typeJson, config.timeoutMs, config.maxRetries);

    loadWebhooksFromDatabase();
    spdlog::info("Added webhook: {} ({})", config.name, config.providerToString());
}

void NotificationService::updateWebhook(const core::WebhookConfig& config) {
    std::lock_guard lock(mutex_);

    std::string severityJson = "[]";
    std::string typeJson = "[]";

    if (!config.severityFilter.empty()) {
        json arr = json::array();
        for (auto s : config.severityFilter) {
            core::Alert temp;
            temp.severity = s;
            arr.push_back(temp.severityToString());
        }
        severityJson = arr.dump();
    }

    if (!config.typeFilter.empty()) {
        json arr = json::array();
        for (auto t : config.typeFilter) {
            core::Alert temp;
            temp.type = t;
            arr.push_back(temp.typeToString());
        }
        typeJson = arr.dump();
    }

    db_->execute(R"(
        UPDATE webhook_configs SET
            name = ?, provider = ?, url = ?, api_token = ?, enabled = ?,
            severity_filter = ?, type_filter = ?, timeout_ms = ?, max_retries = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )",
                 config.name, config.providerToString(), config.url, config.apiToken,
                 config.enabled ? 1 : 0, severityJson, typeJson, config.timeoutMs, config.maxRetries,
                 config.id);

    loadWebhooksFromDatabase();
    spdlog::info("Updated webhook: {} ({})", config.name, config.providerToString());
}

void NotificationService::removeWebhook(int64_t webhookId) {
    std::lock_guard lock(mutex_);

    db_->execute("DELETE FROM webhook_configs WHERE id = ?", webhookId);
    loadWebhooksFromDatabase();
    spdlog::info("Removed webhook with id: {}", webhookId);
}

std::optional<core::WebhookConfig> NotificationService::getWebhook(int64_t webhookId) const {
    std::lock_guard lock(mutex_);

    for (const auto& webhook : webhooks_) {
        if (webhook.id == webhookId) {
            return webhook;
        }
    }
    return std::nullopt;
}

std::vector<core::WebhookConfig> NotificationService::getWebhooks() const {
    std::lock_guard lock(mutex_);
    return webhooks_;
}

void NotificationService::setEnabled(bool enabled) {
    std::lock_guard lock(mutex_);
    enabled_ = enabled;
    spdlog::info("Webhook notifications {}", enabled ? "enabled" : "disabled");
}

bool NotificationService::isEnabled() const {
    std::lock_guard lock(mutex_);
    return enabled_;
}

void NotificationService::subscribe(NotificationCallback callback) {
    std::lock_guard lock(mutex_);
    subscribers_.push_back(std::move(callback));
}

void NotificationService::unsubscribeAll() {
    std::lock_guard lock(mutex_);
    subscribers_.clear();
}

bool NotificationService::testWebhook(const core::WebhookConfig& config) {
    core::Alert testAlert;
    testAlert.id = 0;
    testAlert.hostId = 0;
    testAlert.type = core::AlertType::HostRecovered;
    testAlert.severity = core::AlertSeverity::Info;
    testAlert.title = "Test Alert";
    testAlert.message = "This is a test notification from NetPulse";
    testAlert.timestamp = std::chrono::system_clock::now();
    testAlert.acknowledged = false;

    std::string payload = buildPayload(config, testAlert, "test-host.example.com");
    auto headers = getHeaders(config);

    bool success = false;
    bool completed = false;

    httpClient_->postAsync(config.url, payload, headers, config.timeoutMs,
                           [&success, &completed](const HttpResponse& response) {
                               success = response.success;
                               completed = true;
                           });

    auto start = std::chrono::steady_clock::now();
    while (!completed) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(config.timeoutMs + 1000)) {
            break;
        }
    }

    return success;
}

std::vector<core::WebhookDeliveryLog> NotificationService::getDeliveryLogs(int64_t webhookId,
                                                                            int limit) const {
    std::vector<core::WebhookDeliveryLog> logs;

    auto results = db_->query(R"(
        SELECT id, webhook_id, alert_id, success, http_status, error_message, sent_at
        FROM webhook_delivery_logs
        WHERE webhook_id = ?
        ORDER BY sent_at DESC
        LIMIT ?
    )",
                               webhookId, limit);

    for (const auto& row : results) {
        core::WebhookDeliveryLog log;
        log.id = row.at("id").get<int64_t>();
        log.webhookId = row.at("webhook_id").get<int64_t>();
        log.alertId = row.at("alert_id").get<int64_t>();
        log.success = row.at("success").get<int>() != 0;
        log.httpStatus = row.at("http_status").get<int>();
        log.errorMessage = row.at("error_message").get<std::string>();
        logs.push_back(log);
    }

    return logs;
}

void NotificationService::loadWebhooksFromDatabase() {
    webhooks_.clear();

    auto results = db_->query(R"(
        SELECT id, name, provider, url, api_token, enabled, severity_filter, type_filter, timeout_ms, max_retries
        FROM webhook_configs
        ORDER BY name
    )");

    for (const auto& row : results) {
        core::WebhookConfig config;
        config.id = row.at("id").get<int64_t>();
        config.name = row.at("name").get<std::string>();
        config.provider = core::WebhookConfig::providerFromString(row.at("provider").get<std::string>());
        config.url = row.at("url").get<std::string>();
        config.apiToken = row.at("api_token").get<std::string>();
        config.enabled = row.at("enabled").get<int>() != 0;
        config.timeoutMs = row.at("timeout_ms").get<int>();
        config.maxRetries = row.at("max_retries").get<int>();

        try {
            auto severityJson = json::parse(row.at("severity_filter").get<std::string>());
            for (const auto& s : severityJson) {
                config.severityFilter.push_back(core::Alert::severityFromString(s.get<std::string>()));
            }
        } catch (...) {
        }

        try {
            auto typeJson = json::parse(row.at("type_filter").get<std::string>());
            for (const auto& t : typeJson) {
                config.typeFilter.push_back(core::Alert::typeFromString(t.get<std::string>()));
            }
        } catch (...) {
        }

        webhooks_.push_back(config);
    }

    spdlog::debug("Loaded {} webhooks from database", webhooks_.size());
}

} // namespace netpulse::infra
