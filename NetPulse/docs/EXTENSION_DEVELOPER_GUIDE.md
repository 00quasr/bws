# NetPulse Extension Developer Guide

This guide provides comprehensive documentation for developing extensions (plugins) for NetPulse. The plugin system allows you to extend NetPulse's functionality without modifying core code.

## Table of Contents

1. [Overview](#overview)
2. [Plugin Types](#plugin-types)
3. [Getting Started](#getting-started)
4. [Core Interfaces](#core-interfaces)
5. [Plugin Lifecycle](#plugin-lifecycle)
6. [Plugin Context](#plugin-context)
7. [Specialized Interfaces](#specialized-interfaces)
8. [Configuration](#configuration)
9. [Building and Installing](#building-and-installing)
10. [Best Practices](#best-practices)
11. [Troubleshooting](#troubleshooting)

## Overview

The NetPulse plugin system uses dynamic shared libraries to load extensions at runtime. Each plugin must implement the `IPlugin` interface and export standard C functions for plugin creation and destruction.

### Key Features

- **Dynamic Loading**: Plugins are loaded at runtime from shared libraries
- **Type Safety**: Specialized interfaces for different plugin types
- **Event System**: Publish/subscribe pattern for inter-plugin communication
- **Service Access**: Access to core NetPulse services
- **Configuration**: JSON-based configuration with persistence
- **Version Compatibility**: Automatic version checking and dependency resolution

## Plugin Types

NetPulse supports six plugin types:

| Type | Value | Description |
|------|-------|-------------|
| `NetworkMonitor` | 0 | Custom network monitoring protocols (HTTP, TCP, DNS) |
| `NotificationHandler` | 1 | Custom notification channels (email, SMS, Teams) |
| `DataProcessor` | 2 | Data enrichment, correlation, and transformation |
| `DataExporter` | 3 | Export to external systems (InfluxDB, Prometheus, CSV) |
| `StorageBackend` | 4 | Alternative database backends |
| `Widget` | 5 | Custom dashboard widgets |

## Getting Started

### Plugin Structure

A typical plugin project has this structure:

```
my-plugin/
├── CMakeLists.txt           # Build configuration
├── src/
│   ├── MyPlugin.hpp         # Plugin class declaration
│   └── MyPlugin.cpp         # Plugin implementation
└── plugin.json              # Plugin metadata (optional)
```

### Minimal Plugin Example

Here's a minimal plugin implementation:

```cpp
// MyPlugin.hpp
#pragma once

#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IPluginContext.hpp"

namespace myplugin {

class MyPlugin : public netpulse::core::IPlugin {
public:
    MyPlugin();
    ~MyPlugin() override;

    // Metadata and state
    [[nodiscard]] const netpulse::core::PluginMetadata& metadata() const override;
    [[nodiscard]] netpulse::core::PluginState state() const override;

    // Lifecycle
    bool initialize(netpulse::core::IPluginContext* context) override;
    void shutdown() override;

    // Configuration
    void configure(const nlohmann::json& config) override;
    [[nodiscard]] nlohmann::json configuration() const override;
    [[nodiscard]] nlohmann::json defaultConfiguration() const override;

    // Health and diagnostics
    [[nodiscard]] bool isHealthy() const override;
    [[nodiscard]] std::string statusMessage() const override;
    [[nodiscard]] nlohmann::json diagnostics() const override;

    // Enable/disable
    void enable() override;
    void disable() override;
    [[nodiscard]] bool isEnabled() const override;

private:
    netpulse::core::PluginMetadata metadata_;
    netpulse::core::PluginState state_{netpulse::core::PluginState::Unloaded};
    netpulse::core::IPluginContext* context_{nullptr};
    nlohmann::json config_;
    bool enabled_{true};
};

} // namespace myplugin

// Required C exports for plugin loading
extern "C" {
    netpulse::core::IPlugin* netpulse_create_plugin();
    void netpulse_destroy_plugin(netpulse::core::IPlugin* plugin);
}
```

```cpp
// MyPlugin.cpp
#include "MyPlugin.hpp"

namespace myplugin {

MyPlugin::MyPlugin() {
    // Set up metadata
    metadata_.id = "com.example.myplugin";
    metadata_.name = "My Plugin";
    metadata_.version = "1.0.0";
    metadata_.author = "Your Name";
    metadata_.description = "A custom NetPulse plugin";
    metadata_.license = "MIT";
    metadata_.type = netpulse::core::PluginType::DataProcessor;
    metadata_.minAppVersion = "1.0.0";

    state_ = netpulse::core::PluginState::Loaded;
}

MyPlugin::~MyPlugin() {
    if (state_ >= netpulse::core::PluginState::Initialized) {
        shutdown();
    }
}

const netpulse::core::PluginMetadata& MyPlugin::metadata() const {
    return metadata_;
}

netpulse::core::PluginState MyPlugin::state() const {
    return state_;
}

bool MyPlugin::initialize(netpulse::core::IPluginContext* context) {
    if (!context) {
        return false;
    }

    context_ = context;
    state_ = netpulse::core::PluginState::Initialized;

    // Load default configuration
    config_ = defaultConfiguration();

    context_->logInfo("MyPlugin initialized successfully");

    state_ = netpulse::core::PluginState::Running;
    return true;
}

void MyPlugin::shutdown() {
    if (context_) {
        context_->logInfo("MyPlugin shutting down");
    }
    state_ = netpulse::core::PluginState::Stopped;
    context_ = nullptr;
}

void MyPlugin::configure(const nlohmann::json& config) {
    config_ = config;
    if (context_) {
        context_->logInfo("MyPlugin configuration updated");
    }
}

nlohmann::json MyPlugin::configuration() const {
    return config_;
}

nlohmann::json MyPlugin::defaultConfiguration() const {
    return {
        {"enabled", true},
        {"logLevel", "info"}
    };
}

bool MyPlugin::isHealthy() const {
    return state_ == netpulse::core::PluginState::Running;
}

std::string MyPlugin::statusMessage() const {
    return netpulse::core::pluginStateToString(state_);
}

nlohmann::json MyPlugin::diagnostics() const {
    return {
        {"state", netpulse::core::pluginStateToString(state_)},
        {"enabled", enabled_}
    };
}

void MyPlugin::enable() {
    enabled_ = true;
}

void MyPlugin::disable() {
    enabled_ = false;
}

bool MyPlugin::isEnabled() const {
    return enabled_;
}

} // namespace myplugin

// C exports - required for dynamic loading
extern "C" {

netpulse::core::IPlugin* netpulse_create_plugin() {
    return new myplugin::MyPlugin();
}

void netpulse_destroy_plugin(netpulse::core::IPlugin* plugin) {
    delete plugin;
}

}
```

### Required C Exports

Every plugin **must** export these two C functions:

```cpp
extern "C" {
    // Creates and returns a new plugin instance
    netpulse::core::IPlugin* netpulse_create_plugin();

    // Destroys a plugin instance
    void netpulse_destroy_plugin(netpulse::core::IPlugin* plugin);
}
```

## Core Interfaces

### IPlugin Interface

The `IPlugin` interface (`core/plugin/IPlugin.hpp`) is the base interface that all plugins must implement:

| Method | Description |
|--------|-------------|
| `metadata()` | Returns plugin metadata (id, name, version, etc.) |
| `state()` | Returns current lifecycle state |
| `initialize(context)` | Initializes the plugin with the given context |
| `shutdown()` | Shuts down and releases resources |
| `configure(config)` | Applies JSON configuration |
| `configuration()` | Returns current configuration |
| `defaultConfiguration()` | Returns default configuration |
| `isHealthy()` | Returns true if operating normally |
| `statusMessage()` | Returns human-readable status |
| `diagnostics()` | Returns diagnostic JSON for debugging |
| `enable()` | Enables the plugin |
| `disable()` | Disables the plugin |
| `isEnabled()` | Returns whether the plugin is enabled |

### PluginMetadata Structure

```cpp
struct PluginMetadata {
    std::string id;          // Unique identifier (e.g., "com.company.plugin")
    std::string name;        // Human-readable name
    std::string version;     // Semantic version (e.g., "1.2.3")
    std::string author;      // Author name or organization
    std::string description; // Brief description
    std::string license;     // License type (e.g., "MIT", "Apache-2.0")
    PluginType type;         // Plugin type enum
    std::vector<PluginCapability> capabilities;  // Provided capabilities
    std::vector<PluginDependency> dependencies;  // Required dependencies
    std::string minAppVersion;  // Minimum NetPulse version required
};
```

### PluginCapability Structure

```cpp
struct PluginCapability {
    std::string name;        // Capability name
    std::string version;     // Capability version
    std::string description; // What the capability provides
};
```

### PluginDependency Structure

```cpp
struct PluginDependency {
    std::string pluginName;  // ID of required plugin
    std::string minVersion;  // Minimum required version
    bool required{true};     // Whether dependency is mandatory
};
```

## Plugin Lifecycle

Plugins transition through these states:

```
Unloaded → Loaded → Initialized → Running → Stopped
                                     ↓
                                   Error
```

| State | Description |
|-------|-------------|
| `Unloaded` | Plugin library not loaded |
| `Loaded` | Library loaded, plugin instantiated, not yet initialized |
| `Initialized` | `initialize()` called successfully |
| `Running` | Plugin is actively processing |
| `Stopped` | `shutdown()` called |
| `Error` | An error occurred |

### Lifecycle Flow

1. **Loading**: PluginManager loads the shared library and calls `netpulse_create_plugin()`
2. **Initialization**: PluginManager calls `initialize()` with the plugin context
3. **Running**: Plugin processes events, handles requests
4. **Configuration**: `configure()` can be called at any time to update settings
5. **Shutdown**: PluginManager calls `shutdown()` when unloading

## Plugin Context

The `IPluginContext` interface provides plugins access to NetPulse services and facilities:

### Service Access

```cpp
// Get a service by name (type-safe)
auto* pingService = context->getService<IPingService>("pingService");

// Check if a service exists
if (context->hasService("database")) {
    // Service is available
}

// Register your own service for other plugins
context->registerService("myService", myServicePtr);

// Unregister a service
context->unregisterService("myService");
```

### Available Core Services

| Service Name | Type | Description |
|--------------|------|-------------|
| `pingService` | `IPingService*` | ICMP ping monitoring |
| `portScanner` | `IPortScanner*` | Port scanning |
| `database` | `IDatabase*` | SQLite database access |
| `notificationService` | `INotificationService*` | Notification delivery |
| `asioContext` | `asio::io_context*` | Asio event loop |

### Event System

The plugin system includes a publish/subscribe event mechanism:

```cpp
// Subscribe to an event
int64_t subscriptionId = context->subscribe("host.status.changed",
    [this](const std::string& eventName, const std::any& data) {
        // Handle the event
        auto hostData = std::any_cast<nlohmann::json>(data);
        context_->logInfo("Host status changed: " + hostData.dump());
    });

// Publish an event
nlohmann::json eventData = {{"hostId", 123}, {"status", "online"}};
context->publish("myplugin.host.processed", eventData);

// Unsubscribe when done
context->unsubscribe(subscriptionId);
```

### Directory Access

```cpp
std::string configPath = context->configDir();   // Configuration directory
std::string dataPath = context->dataDir();       // Data storage directory
std::string pluginPath = context->pluginDir();   // Plugins directory
```

### Logging

```cpp
context->logDebug("Debug message");
context->logInfo("Informational message");
context->logWarning("Warning message");
context->logError("Error message");

// Or use the generic method
context->log("info", "Custom log message");
```

### Version Information

```cpp
std::string appVersion = context->appVersion();  // e.g., "1.5.0"
```

## Specialized Interfaces

Beyond the base `IPlugin` interface, plugins can implement specialized interfaces based on their type.

### INetworkMonitorPlugin

For custom network monitoring protocols (`core/plugin/PluginHooks.hpp`):

```cpp
class MyHttpMonitor : public netpulse::core::IPlugin,
                      public netpulse::core::INetworkMonitorPlugin {
public:
    // INetworkMonitorPlugin interface
    [[nodiscard]] std::string monitorType() const override {
        return "http";
    }

    [[nodiscard]] bool supportsAddress(const std::string& address) const override {
        return address.find("http://") == 0 || address.find("https://") == 0;
    }

    void startMonitoring(const netpulse::core::Host& host,
                         MonitorCallback callback) override {
        // Start periodic HTTP checks for this host
    }

    void stopMonitoring(int64_t hostId) override {
        // Stop monitoring the specified host
    }

    void stopAllMonitoring() override {
        // Stop all monitoring operations
    }

    [[nodiscard]] bool isMonitoring(int64_t hostId) const override {
        // Check if monitoring is active for the host
    }

    std::future<netpulse::core::NetworkMonitorResult> checkAsync(
            const std::string& address,
            std::chrono::milliseconds timeout) override {
        // Perform async HTTP health check
    }
};
```

### INotificationPlugin

For custom notification channels:

```cpp
class MyTeamsNotifier : public netpulse::core::IPlugin,
                        public netpulse::core::INotificationPlugin {
public:
    [[nodiscard]] std::string notificationType() const override {
        return "teams";
    }

    [[nodiscard]] nlohmann::json configurationSchema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"webhookUrl", {{"type", "string"}, {"format", "uri"}}},
                {"channel", {{"type", "string"}}}
            }},
            {"required", {"webhookUrl"}}
        };
    }

    netpulse::core::PluginNotificationResult send(
            const netpulse::core::NotificationPayload& payload,
            const nlohmann::json& endpointConfig) override {
        // Send notification to Microsoft Teams
    }

    netpulse::core::PluginNotificationResult testConnection(
            const nlohmann::json& endpointConfig) override {
        // Test Teams webhook connectivity
    }

    [[nodiscard]] std::string formatPayload(
            const netpulse::core::NotificationPayload& payload,
            const nlohmann::json& endpointConfig) const override {
        // Format payload as Teams Adaptive Card
    }
};
```

### IDataProcessorPlugin

For data processing and enrichment:

```cpp
class MyDataEnricher : public netpulse::core::IPlugin,
                       public netpulse::core::IDataProcessorPlugin {
public:
    [[nodiscard]] std::vector<std::string> supportedDataTypes() const override {
        return {"ping_result", "alert"};
    }

    netpulse::core::ProcessedData process(
            const std::string& dataType,
            const nlohmann::json& data) override {
        netpulse::core::ProcessedData result;
        result.dataType = dataType;
        result.originalData = data;
        result.processedAt = std::chrono::system_clock::now();

        // Enrich the data
        result.enrichedData = data;
        result.enrichedData["enriched_by"] = metadata().id;
        result.tags = {"enriched", dataType};

        return result;
    }

    [[nodiscard]] bool canProcess(const std::string& dataType) const override {
        auto types = supportedDataTypes();
        return std::find(types.begin(), types.end(), dataType) != types.end();
    }

    // Optional event hooks
    void onPingResult(const netpulse::core::PingResult& result) override {
        // Called when a ping result is received
    }

    void onAlert(const netpulse::core::Alert& alert) override {
        // Called when an alert is generated
    }

    void onPortScanComplete(const netpulse::core::PortScanResult& result) override {
        // Called when a port scan completes
    }
};
```

### IDataExporterPlugin

For exporting data to external systems:

```cpp
class MyInfluxExporter : public netpulse::core::IPlugin,
                         public netpulse::core::IDataExporterPlugin {
public:
    [[nodiscard]] std::string exporterType() const override {
        return "influxdb";
    }

    [[nodiscard]] std::vector<std::string> supportedFormats() const override {
        return {"line_protocol", "json"};
    }

    netpulse::core::ExportResult exportData(
            const std::string& format,
            const nlohmann::json& data,
            const nlohmann::json& options) override {
        // Export to InfluxDB
    }

    netpulse::core::ExportResult exportToFile(
            const std::string& format,
            const nlohmann::json& data,
            const std::string& filePath) override {
        // Export to file
    }

    [[nodiscard]] bool testConnection(const nlohmann::json& options) override {
        // Test InfluxDB connectivity
    }
};
```

## Configuration

### Plugin Configuration

Configuration is handled via JSON:

```cpp
// In your plugin
nlohmann::json MyPlugin::defaultConfiguration() const {
    return {
        {"enabled", true},
        {"logLevel", "info"},
        {"refreshInterval", 30},
        {"endpoints", nlohmann::json::array()}
    };
}

void MyPlugin::configure(const nlohmann::json& config) {
    // Merge with defaults for any missing keys
    auto defaults = defaultConfiguration();
    config_ = defaults;
    config_.merge_patch(config);

    // Apply configuration changes
    if (config_.contains("logLevel")) {
        setLogLevel(config_["logLevel"]);
    }
}
```

### Plugin Metadata JSON File

You can also define metadata in a JSON file (`plugin.json`):

```json
{
    "id": "com.example.myplugin",
    "name": "My Plugin",
    "version": "1.0.0",
    "author": "Your Name",
    "description": "A custom NetPulse plugin",
    "license": "MIT",
    "type": 2,
    "minAppVersion": "1.0.0",
    "capabilities": [
        {
            "name": "data-processor",
            "version": "1.0.0",
            "description": "Processes monitoring data"
        }
    ],
    "dependencies": [
        {
            "pluginName": "com.netpulse.core",
            "minVersion": "1.0.0",
            "required": true
        }
    ],
    "enabled": true,
    "configuration": {
        "logLevel": "info",
        "refreshInterval": 30
    }
}
```

## Building and Installing

### CMakeLists.txt Example

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyNetPulsePlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find NetPulse plugin interface
find_package(netpulse_plugin_interface REQUIRED)

# Create shared library
add_library(myplugin SHARED
    src/MyPlugin.cpp
)

target_include_directories(myplugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(myplugin PRIVATE
    netpulse::plugin_interface
)

# Set output name without lib prefix
set_target_properties(myplugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "myplugin"
)

# Platform-specific settings
if(WIN32)
    set_target_properties(myplugin PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(myplugin PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(myplugin PROPERTIES SUFFIX ".so")
endif()

# Install to plugins directory
install(TARGETS myplugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)
```

### Building

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

# Install (copies to plugins directory)
cmake --install .
```

### Installation

Copy your compiled plugin to NetPulse's plugins directory:

- **Linux**: `~/.local/share/netpulse/plugins/` or `/usr/local/share/netpulse/plugins/`
- **macOS**: `~/Library/Application Support/NetPulse/plugins/`
- **Windows**: `%APPDATA%\NetPulse\plugins\`

## Best Practices

### Thread Safety

- Use mutexes to protect shared state
- The plugin context is thread-safe for service access and event publishing
- Be careful with atomic operations for simple flags

```cpp
class MyPlugin : public netpulse::core::IPlugin {
private:
    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{true};
    int64_t processedCount_{0};

public:
    void processData() {
        if (!enabled_.load()) return;

        std::lock_guard<std::mutex> lock(mutex_);
        processedCount_++;
        // ... process data
    }
};
```

### Error Handling

- Never throw exceptions from C export functions
- Return false from `initialize()` on failure
- Set state to `Error` when something goes wrong
- Provide meaningful error messages via `statusMessage()`

```cpp
bool MyPlugin::initialize(netpulse::core::IPluginContext* context) {
    try {
        if (!context) {
            state_ = netpulse::core::PluginState::Error;
            errorMessage_ = "Null context provided";
            return false;
        }

        // Initialization logic...

        return true;
    } catch (const std::exception& e) {
        state_ = netpulse::core::PluginState::Error;
        errorMessage_ = e.what();
        return false;
    }
}
```

### Resource Management

- Use RAII for all resources
- Release all resources in `shutdown()`
- Implement proper destructor that calls `shutdown()` if needed

```cpp
MyPlugin::~MyPlugin() {
    if (state_ >= netpulse::core::PluginState::Initialized) {
        shutdown();
    }
}

void MyPlugin::shutdown() {
    // Stop any background tasks
    stopBackgroundWorker();

    // Unsubscribe from events
    if (subscriptionId_ != -1 && context_) {
        context_->unsubscribe(subscriptionId_);
        subscriptionId_ = -1;
    }

    // Release resources
    connection_.reset();

    if (context_) {
        context_->logInfo("MyPlugin shut down successfully");
    }

    state_ = netpulse::core::PluginState::Stopped;
    context_ = nullptr;
}
```

### Version Compatibility

- Always set `minAppVersion` to the minimum NetPulse version your plugin supports
- Use semantic versioning for your plugin version
- Check `context->appVersion()` if you need runtime version checks

### Naming Conventions

- **Plugin ID**: Use reverse domain notation (e.g., `com.company.pluginname`)
- **Event names**: Use dot-separated hierarchical names (e.g., `myplugin.data.processed`)
- **Service names**: Use camelCase (e.g., `myCustomService`)

## Troubleshooting

### Common Issues

**Plugin not loading**
- Verify the shared library exports `netpulse_create_plugin` and `netpulse_destroy_plugin`
- Check library dependencies with `ldd` (Linux) or `otool -L` (macOS)
- Ensure the plugin is in the correct plugins directory

**Initialization fails**
- Check that `minAppVersion` is compatible with the running NetPulse version
- Verify all required dependencies are loaded
- Review logs for specific error messages

**Service not found**
- Ensure the service name is spelled correctly (case-sensitive)
- Verify the service is registered before your plugin initializes
- Check if the service provider plugin is loaded

**Events not received**
- Confirm the event name matches exactly (case-sensitive)
- Verify you subscribed before the events are published
- Check that the subscription ID is valid

### Debugging

Enable verbose logging in your plugin:

```cpp
nlohmann::json MyPlugin::defaultConfiguration() const {
    return {
        {"logLevel", "debug"}  // Set to debug for development
    };
}
```

Add diagnostics information:

```cpp
nlohmann::json MyPlugin::diagnostics() const {
    return {
        {"state", netpulse::core::pluginStateToString(state_)},
        {"enabled", enabled_.load()},
        {"lastError", lastError_},
        {"processedCount", processedCount_},
        {"activeConnections", activeConnections_},
        {"uptime", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_).count()}
    };
}
```

### Getting Help

- Check the [Example Plugin](../plugins/example/) for a complete working example
- Review the API documentation generated by Doxygen
- File issues on the [GitHub repository](https://github.com/00quasr/bws)

## Complete Example

See the [Example Plugin](../plugins/example/) directory for a complete working plugin that demonstrates:

- Basic `IPlugin` implementation
- `IDataProcessorPlugin` specialization
- Event publishing
- Configuration handling
- Diagnostics reporting
- Thread-safe state management
