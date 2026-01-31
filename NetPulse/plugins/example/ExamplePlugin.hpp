#pragma once

#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IPluginContext.hpp"
#include "core/plugin/PluginHooks.hpp"

#include <atomic>
#include <mutex>

namespace netpulse::plugins {

class ExamplePlugin : public core::IPlugin, public core::IDataProcessorPlugin {
public:
    ExamplePlugin();
    ~ExamplePlugin() override;

    [[nodiscard]] const core::PluginMetadata& metadata() const override { return metadata_; }
    [[nodiscard]] core::PluginState state() const override { return state_; }

    bool initialize(core::IPluginContext* context) override;
    void shutdown() override;

    void configure(const nlohmann::json& config) override;
    [[nodiscard]] nlohmann::json configuration() const override { return config_; }
    [[nodiscard]] nlohmann::json defaultConfiguration() const override;

    [[nodiscard]] bool isHealthy() const override { return state_ == core::PluginState::Running; }
    [[nodiscard]] std::string statusMessage() const override;
    [[nodiscard]] nlohmann::json diagnostics() const override;

    void enable() override;
    void disable() override;
    [[nodiscard]] bool isEnabled() const override { return enabled_; }

    [[nodiscard]] std::vector<std::string> supportedDataTypes() const override;
    core::ProcessedData process(const std::string& dataType, const nlohmann::json& data) override;
    [[nodiscard]] bool canProcess(const std::string& dataType) const override;

    void onPingResult(const core::PingResult& result) override;
    void onAlert(const core::Alert& alert) override;

private:
    core::PluginMetadata metadata_;
    core::PluginState state_{core::PluginState::Unloaded};
    core::IPluginContext* context_{nullptr};
    nlohmann::json config_;
    std::atomic<bool> enabled_{true};

    mutable std::mutex statsMutex_;
    int64_t processedCount_{0};
    int64_t pingResultsReceived_{0};
    int64_t alertsReceived_{0};
};

} // namespace netpulse::plugins

extern "C" {
    netpulse::core::IPlugin* netpulse_create_plugin();
    void netpulse_destroy_plugin(netpulse::core::IPlugin* plugin);
}
