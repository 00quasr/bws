#pragma once

#include "core/types/Alert.hpp"
#include "infrastructure/crypto/SecureStorage.hpp"

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace netpulse::infra {

/**
 * @brief Configuration for a single plugin.
 */
struct PluginConfig {
    std::string id;            ///< Unique plugin identifier.
    std::string path;          ///< Path to plugin shared library.
    bool enabled{true};        ///< Whether the plugin is enabled.
    nlohmann::json settings;   ///< Plugin-specific settings.
};

/**
 * @brief Application configuration settings.
 *
 * Contains all user-configurable application settings including
 * monitoring defaults, alert thresholds, UI preferences, and feature flags.
 */
struct AppConfig {
    // General settings
    std::string theme{"dark"};     ///< UI theme ("dark" or "light").
    bool startMinimized{false};    ///< Start application minimized.
    bool minimizeToTray{true};     ///< Minimize to system tray.
    bool startOnLogin{false};      ///< Start application at system login.

    // Monitoring defaults
    int defaultPingIntervalSeconds{30};  ///< Default ping interval in seconds.
    int defaultWarningThresholdMs{100};  ///< Warning threshold in milliseconds.
    int defaultCriticalThresholdMs{500}; ///< Critical threshold in milliseconds.

    // Alert settings
    core::AlertThresholds alertThresholds; ///< Alert threshold configuration.
    bool desktopNotifications{true};       ///< Enable desktop notifications.
    bool soundAlerts{false};               ///< Enable sound alerts.

    // Data retention
    int dataRetentionDays{30};   ///< Days to retain historical data.
    bool autoCleanup{true};      ///< Automatically clean old data.

    // Port scanner defaults
    int portScanConcurrency{100};  ///< Maximum concurrent port scans.
    int portScanTimeoutMs{1000};   ///< Port scan timeout in milliseconds.

    // Window state
    int windowX{100};            ///< Window X position.
    int windowY{100};            ///< Window Y position.
    int windowWidth{1200};       ///< Window width in pixels.
    int windowHeight{800};       ///< Window height in pixels.
    bool windowMaximized{false}; ///< Window maximized state.

    // Webhook notifications
    bool webhooksEnabled{true};   ///< Enable webhook notifications.
    int webhookTimeoutMs{5000};   ///< Webhook request timeout in milliseconds.
    int webhookMaxRetries{3};     ///< Maximum webhook delivery retries.

    // REST API settings
    bool restApiEnabled{false};   ///< Enable REST API server.
    uint16_t restApiPort{8080};   ///< REST API server port.

    // Plugin settings
    bool pluginsEnabled{true};           ///< Enable plugin system.
    std::vector<PluginConfig> plugins;   ///< Plugin configurations.
};

/**
 * @brief Manages application configuration persistence.
 *
 * Handles loading and saving of application configuration from JSON files.
 * Supports secure storage for sensitive values like API keys.
 */
class ConfigManager {
public:
    /**
     * @brief Constructs a ConfigManager for the specified config directory.
     * @param configDir Path to the configuration directory.
     */
    explicit ConfigManager(const std::filesystem::path& configDir);

    /**
     * @brief Loads configuration from disk.
     * @return True if loaded successfully, false otherwise.
     */
    bool load();

    /**
     * @brief Saves configuration to disk.
     * @return True if saved successfully, false otherwise.
     */
    bool save();

    /**
     * @brief Returns a mutable reference to the configuration.
     * @return Reference to AppConfig.
     */
    AppConfig& config() { return config_; }

    /**
     * @brief Returns a const reference to the configuration.
     * @return Const reference to AppConfig.
     */
    const AppConfig& config() const { return config_; }

    /**
     * @brief Stores a value in secure encrypted storage.
     * @param key Key name for the value.
     * @param value Value to store (will be encrypted).
     */
    void setSecureValue(const std::string& key, const std::string& value);

    /**
     * @brief Retrieves a value from secure storage.
     * @param key Key name of the value.
     * @return Decrypted value if found, nullopt otherwise.
     */
    std::optional<std::string> getSecureValue(const std::string& key);

    /**
     * @brief Returns the path to the configuration file.
     * @return Path to config.json.
     */
    std::filesystem::path configPath() const { return configPath_; }

    /**
     * @brief Returns the path to the database file.
     * @return Path to the SQLite database.
     */
    std::filesystem::path databasePath() const;

    /**
     * @brief Returns the path to the plugins directory.
     * @return Path to plugins directory.
     */
    std::filesystem::path pluginDir() const;

    /**
     * @brief Returns the path to the plugin state file.
     * @return Path to plugin state JSON file.
     */
    std::filesystem::path pluginStatePath() const;

    /**
     * @brief Returns the configuration directory path as a string.
     * @return Configuration directory path.
     */
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
