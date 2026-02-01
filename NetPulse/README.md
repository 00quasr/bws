# NetPulse

A cross-platform network monitoring dashboard built with C++20 and Qt6.

NetPulse is designed as a comprehensive Network Operations Center (NOC) tool for monitoring hosts, tracking latency, scanning ports, and managing network alerts in real-time.

## Features

- **Host Management** - Add, edit, remove monitored hosts with configurable intervals
- **ICMP Ping Monitoring** - Real-time latency measurement and host status tracking
- **Port Scanning** - TCP connect scan with concurrent scanning (100+ ports simultaneously)
- **Latency Visualization** - Real-time charts and sparkline widgets per host
- **Metrics Storage** - SQLite-based persistent database with WAL mode
- **Alert System** - Severity-based alerts (Info/Warning/Critical) with thresholds
- **Secure Credential Storage** - libsodium encryption for sensitive data
- **Host Groups** - Organize hosts into collapsible groups with drag-and-drop
- **Dashboard Widgets** - Customizable dashboard with layout persistence
- **System Tray** - Minimize to tray with single-instance enforcement
- **Cross-Platform** - Linux, macOS, and Windows support

## Requirements

### Build Dependencies

**Ubuntu/Debian:**
```bash
sudo apt install build-essential cmake clang clang-format \
    qt6-base-dev qt6-charts-dev libsqlite3-dev libsodium-dev \
    nlohmann-json3-dev libspdlog-dev catch2 libasio-dev pkg-config
```

**macOS:**
```bash
brew install cmake llvm qt6 asio libsodium nlohmann-json spdlog catch2 sqlite3
```

**vcpkg (cross-platform):**
```bash
vcpkg install asio libsodium nlohmann-json spdlog sqlite3 catch2
```

## Build

```bash
cd NetPulse

# Debug
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Release
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Run

```bash
./build/netpulse
```

**Linux Note:** ICMP ping requires `CAP_NET_RAW` capability:
```bash
sudo setcap cap_net_raw+ep ./build/netpulse
```

## Tests

```bash
ctest --test-dir build --output-on-failure
# or
./build/netpulse_tests
```

## Architecture

NetPulse follows a layered MVVM architecture:

```
┌─────────────────────────────────────┐
│         UI Layer (Qt6)              │
│  MainWindow, Dashboard, Widgets     │
├─────────────────────────────────────┤
│         ViewModels (MVVM)           │
│  Dashboard, HostMonitor, Alerts     │
├─────────────────────────────────────┤
│       Infrastructure Layer          │
│  Network (Asio), Database (SQLite), │
│  Crypto (libsodium), Config         │
├─────────────────────────────────────┤
│       Core Domain Layer             │
│  Host, HostGroup, PingResult, Alert │
└─────────────────────────────────────┘
```

## Project Structure

```
NetPulse/
├── src/
│   ├── app/             # Application lifecycle
│   ├── core/            # Domain types & interfaces
│   │   ├── types/       # Host, HostGroup, PingResult, Alert
│   │   └── services/    # IPingService, IPortScanner, IAlertService
│   ├── infrastructure/  # Implementation layer
│   │   ├── network/     # PingService, PortScanner, AsioContext
│   │   ├── database/    # Database, Repositories
│   │   ├── crypto/      # SecureStorage
│   │   └── config/      # ConfigManager
│   ├── viewmodels/      # MVVM ViewModels
│   └── ui/              # Qt6 windows, widgets, themes
│       ├── windows/     # MainWindow, Dialogs
│       └── widgets/     # HostList, LatencyChart, Sparkline, Dashboard
├── tests/unit/          # Catch2 tests
└── CMakeLists.txt
```

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| Build | CMake 3.20+ |
| GUI | Qt6 (Widgets, Charts) |
| Async I/O | Asio |
| Database | SQLite3 |
| Crypto | libsodium |
| JSON | nlohmann_json |
| Logging | spdlog |
| Testing | Catch2 |

## Platform Notes

| Platform | ICMP Implementation |
|----------|---------------------|
| Linux | `SOCK_RAW` (requires `CAP_NET_RAW`) |
| macOS | `SOCK_DGRAM` (non-privileged) |
| Windows | `IcmpSendEcho2` via iphlpapi |

## Roadmap

- [ ] Traceroute with hop-by-hop latency
- [ ] TCP connection latency measurement
- [ ] Bandwidth monitoring for local interfaces
- [ ] DNS resolution monitoring
- [ ] Full-screen NOC display mode
- [ ] Network topology visualization
- [ ] Scheduled port scans with diff reports
- [ ] Webhook notifications (Slack, Discord, PagerDuty)
- [ ] REST API for external integrations
- [ ] SNMP v2c/v3 monitoring
- [ ] Plugin system

## License

MIT
