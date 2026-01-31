#include "infrastructure/config/ConfigManager.hpp"

#include <spdlog/spdlog.h>

#include <fstream>

namespace netpulse::infra {

ConfigManager::ConfigManager(const std::filesystem::path& configDir) : configDir_(configDir) {
    if (!std::filesystem::exists(configDir_)) {
        std::filesystem::create_directories(configDir_);
    }

    configPath_ = configDir_ / "config.json";
    auto keyPath = configDir_ / ".key";
    secureStorage_ = std::make_unique<SecureStorage>(keyPath);
}

bool ConfigManager::load() {
    if (!std::filesystem::exists(configPath_)) {
        spdlog::info("Config file not found, using defaults");
        return save();
    }

    try {
        std::ifstream file(configPath_);
        if (!file) {
            spdlog::error("Failed to open config file: {}", configPath_.string());
            return false;
        }

        nlohmann::json j;
        file >> j;
        fromJson(j);

        spdlog::info("Loaded configuration from {}", configPath_.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load config: {}", e.what());
        return false;
    }
}

bool ConfigManager::save() {
    try {
        auto j = toJson();

        std::ofstream file(configPath_);
        if (!file) {
            spdlog::error("Failed to open config file for writing: {}", configPath_.string());
            return false;
        }

        file << j.dump(2);
        spdlog::debug("Saved configuration to {}", configPath_.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config: {}", e.what());
        return false;
    }
}

nlohmann::json ConfigManager::toJson() const {
    nlohmann::json j;

    // General
    j["general"]["theme"] = config_.theme;
    j["general"]["start_minimized"] = config_.startMinimized;
    j["general"]["minimize_to_tray"] = config_.minimizeToTray;
    j["general"]["start_on_login"] = config_.startOnLogin;

    // Monitoring defaults
    j["monitoring"]["default_ping_interval_seconds"] = config_.defaultPingIntervalSeconds;
    j["monitoring"]["default_warning_threshold_ms"] = config_.defaultWarningThresholdMs;
    j["monitoring"]["default_critical_threshold_ms"] = config_.defaultCriticalThresholdMs;

    // Alerts
    j["alerts"]["latency_warning_ms"] = config_.alertThresholds.latencyWarningMs;
    j["alerts"]["latency_critical_ms"] = config_.alertThresholds.latencyCriticalMs;
    j["alerts"]["packet_loss_warning_percent"] = config_.alertThresholds.packetLossWarningPercent;
    j["alerts"]["packet_loss_critical_percent"] = config_.alertThresholds.packetLossCriticalPercent;
    j["alerts"]["consecutive_failures_for_down"] =
        config_.alertThresholds.consecutiveFailuresForDown;
    j["alerts"]["desktop_notifications"] = config_.desktopNotifications;
    j["alerts"]["sound_alerts"] = config_.soundAlerts;

    // Data retention
    j["data"]["retention_days"] = config_.dataRetentionDays;
    j["data"]["auto_cleanup"] = config_.autoCleanup;

    // Port scanner
    j["port_scanner"]["concurrency"] = config_.portScanConcurrency;
    j["port_scanner"]["timeout_ms"] = config_.portScanTimeoutMs;

    // Window state
    j["window"]["x"] = config_.windowX;
    j["window"]["y"] = config_.windowY;
    j["window"]["width"] = config_.windowWidth;
    j["window"]["height"] = config_.windowHeight;
    j["window"]["maximized"] = config_.windowMaximized;

    // Webhooks
    j["webhooks"]["enabled"] = config_.webhooksEnabled;
    j["webhooks"]["timeout_ms"] = config_.webhookTimeoutMs;
    j["webhooks"]["max_retries"] = config_.webhookMaxRetries;

    // Encrypted values
    if (!secureValues_.empty()) {
        j["secure"] = secureValues_;
    }

    return j;
}

void ConfigManager::fromJson(const nlohmann::json& j) {
    // General
    if (j.contains("general")) {
        const auto& g = j["general"];
        config_.theme = g.value("theme", "dark");
        config_.startMinimized = g.value("start_minimized", false);
        config_.minimizeToTray = g.value("minimize_to_tray", true);
        config_.startOnLogin = g.value("start_on_login", false);
    }

    // Monitoring
    if (j.contains("monitoring")) {
        const auto& m = j["monitoring"];
        config_.defaultPingIntervalSeconds = m.value("default_ping_interval_seconds", 30);
        config_.defaultWarningThresholdMs = m.value("default_warning_threshold_ms", 100);
        config_.defaultCriticalThresholdMs = m.value("default_critical_threshold_ms", 500);
    }

    // Alerts
    if (j.contains("alerts")) {
        const auto& a = j["alerts"];
        config_.alertThresholds.latencyWarningMs = a.value("latency_warning_ms", 100);
        config_.alertThresholds.latencyCriticalMs = a.value("latency_critical_ms", 500);
        config_.alertThresholds.packetLossWarningPercent =
            a.value("packet_loss_warning_percent", 5.0);
        config_.alertThresholds.packetLossCriticalPercent =
            a.value("packet_loss_critical_percent", 20.0);
        config_.alertThresholds.consecutiveFailuresForDown =
            a.value("consecutive_failures_for_down", 3);
        config_.desktopNotifications = a.value("desktop_notifications", true);
        config_.soundAlerts = a.value("sound_alerts", false);
    }

    // Data
    if (j.contains("data")) {
        const auto& d = j["data"];
        config_.dataRetentionDays = d.value("retention_days", 30);
        config_.autoCleanup = d.value("auto_cleanup", true);
    }

    // Port scanner
    if (j.contains("port_scanner")) {
        const auto& p = j["port_scanner"];
        config_.portScanConcurrency = p.value("concurrency", 100);
        config_.portScanTimeoutMs = p.value("timeout_ms", 1000);
    }

    // Window state
    if (j.contains("window")) {
        const auto& w = j["window"];
        config_.windowX = w.value("x", 100);
        config_.windowY = w.value("y", 100);
        config_.windowWidth = w.value("width", 1200);
        config_.windowHeight = w.value("height", 800);
        config_.windowMaximized = w.value("maximized", false);
    }

    // Webhooks
    if (j.contains("webhooks")) {
        const auto& wh = j["webhooks"];
        config_.webhooksEnabled = wh.value("enabled", true);
        config_.webhookTimeoutMs = wh.value("timeout_ms", 5000);
        config_.webhookMaxRetries = wh.value("max_retries", 3);
    }

    // Secure values
    if (j.contains("secure")) {
        secureValues_ = j["secure"];
    }
}

void ConfigManager::setSecureValue(const std::string& key, const std::string& value) {
    auto encrypted = secureStorage_->encrypt(value);
    if (!encrypted.empty()) {
        secureValues_[key] = encrypted;
        save();
    }
}

std::optional<std::string> ConfigManager::getSecureValue(const std::string& key) {
    if (!secureValues_.contains(key)) {
        return std::nullopt;
    }

    auto encrypted = secureValues_[key].get<std::string>();
    return secureStorage_->decrypt(encrypted);
}

std::filesystem::path ConfigManager::databasePath() const {
    return configDir_ / "netpulse.db";
}

} // namespace netpulse::infra
