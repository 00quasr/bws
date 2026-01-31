#pragma once

#include "core/types/Alert.hpp"
#include "infrastructure/crypto/SecureStorage.hpp"

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace netpulse::infra {

struct AppConfig {
    // General settings
    std::string theme{"dark"};
    bool startMinimized{false};
    bool minimizeToTray{true};
    bool startOnLogin{false};

    // Monitoring defaults
    int defaultPingIntervalSeconds{30};
    int defaultWarningThresholdMs{100};
    int defaultCriticalThresholdMs{500};

    // Alert settings
    core::AlertThresholds alertThresholds;
    bool desktopNotifications{true};
    bool soundAlerts{false};

    // Data retention
    int dataRetentionDays{30};
    bool autoCleanup{true};

    // Port scanner defaults
    int portScanConcurrency{100};
    int portScanTimeoutMs{1000};

    // Window state
    int windowX{100};
    int windowY{100};
    int windowWidth{1200};
    int windowHeight{800};
    bool windowMaximized{false};
};

class ConfigManager {
public:
    explicit ConfigManager(const std::filesystem::path& configDir);

    bool load();
    bool save();

    AppConfig& config() { return config_; }
    const AppConfig& config() const { return config_; }

    // Secure storage for sensitive data
    void setSecureValue(const std::string& key, const std::string& value);
    std::optional<std::string> getSecureValue(const std::string& key);

    std::filesystem::path configPath() const { return configPath_; }
    std::filesystem::path databasePath() const;
    std::string configDir() const { return configDir_.string(); }

private:
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    std::filesystem::path configDir_;
    std::filesystem::path configPath_;
    AppConfig config_;
    std::unique_ptr<SecureStorage> secureStorage_;
    nlohmann::json secureValues_;
};

} // namespace netpulse::infra
