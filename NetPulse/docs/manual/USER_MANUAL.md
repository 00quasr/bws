# NetPulse User Manual

**Version 1.0.0**

NetPulse is a cross-platform network monitoring dashboard built with C++20 and Qt6. It serves as a comprehensive Network Operations Center (NOC) tool for monitoring hosts, tracking latency, scanning ports, and managing network alerts in real-time.

---

## Table of Contents

1. [Getting Started](#getting-started)
   - [Installation](#installation)
   - [First Launch](#first-launch)
   - [System Requirements](#system-requirements)
2. [Main Interface](#main-interface)
   - [Window Layout](#window-layout)
   - [Menu Bar](#menu-bar)
   - [Toolbar](#toolbar)
   - [Status Bar](#status-bar)
3. [Host Management](#host-management)
   - [Adding a Host](#adding-a-host)
   - [Editing a Host](#editing-a-host)
   - [Removing a Host](#removing-a-host)
   - [Host Groups](#host-groups)
4. [Dashboard](#dashboard)
   - [Dashboard Overview](#dashboard-overview)
   - [Available Widgets](#available-widgets)
   - [Customizing Layout](#customizing-layout)
5. [Host Details](#host-details)
   - [Latency Chart](#latency-chart)
   - [Sparkline Indicators](#sparkline-indicators)
6. [Port Scanner](#port-scanner)
   - [Running a Scan](#running-a-scan)
   - [Interpreting Results](#interpreting-results)
7. [Alerts](#alerts)
   - [Alert Types](#alert-types)
   - [Managing Alerts](#managing-alerts)
8. [NOC Display Mode](#noc-display-mode)
   - [Entering NOC Mode](#entering-noc-mode)
   - [NOC Interface](#noc-interface)
9. [Settings](#settings)
   - [General Settings](#general-settings)
   - [Monitoring Settings](#monitoring-settings)
   - [Alert Settings](#alert-settings)
   - [Data Retention](#data-retention)
10. [Data Export](#data-export)
11. [System Tray](#system-tray)
12. [Keyboard Shortcuts](#keyboard-shortcuts)
13. [Troubleshooting](#troubleshooting)

---

## Getting Started

### Installation

#### Building from Source

**Prerequisites (Ubuntu/Debian):**
```bash
sudo apt install build-essential cmake clang clang-format \
    qt6-base-dev qt6-charts-dev libsqlite3-dev libsodium-dev \
    nlohmann-json3-dev libspdlog-dev catch2 libasio-dev pkg-config
```

**Prerequisites (macOS):**
```bash
brew install cmake llvm qt6 asio libsodium nlohmann-json spdlog catch2 sqlite3
```

**Build Steps:**
```bash
cd NetPulse
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Linux ICMP Permissions:**
On Linux, ICMP ping requires special permissions. Grant the capability:
```bash
sudo setcap cap_net_raw+ep ./build/netpulse
```

### First Launch

Run the application:
```bash
./build/netpulse
```

On first launch, NetPulse will:
1. Create a configuration directory at `~/.local/share/NetPulse/NetPulse/`
2. Initialize the SQLite database
3. Generate a secure encryption key for credential storage
4. Display the main window with an empty host list

![First Launch](screenshots/01_first_launch.png)
*Screenshot: The main window on first launch with an empty host list and default dashboard*

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Linux, macOS 10.15+, Windows 10 | Ubuntu 22.04+, macOS 12+ |
| RAM | 256 MB | 512 MB |
| Storage | 50 MB | 100 MB |
| Display | 1024x768 | 1920x1080 |

---

## Main Interface

### Window Layout

The NetPulse main window consists of several key areas:

```
┌─────────────────────────────────────────────────────────────┐
│  Menu Bar: File | View | Tools | Help                       │
├─────────────────────────────────────────────────────────────┤
│  Toolbar: [Add Host] [Remove Host] | [Port Scan] | [Settings]│
├──────────────────┬──────────────────────────────────────────┤
│                  │                                          │
│   Host List      │   Tab Area                               │
│   ┌───────────┐  │   ┌────────────────────────────────────┐ │
│   │ Host 1    │  │   │  [Dashboard] [Host Details]        │ │
│   │ ░░░░░░░░  │  │   │  [Interfaces] [Alerts]             │ │
│   │ Host 2    │  │   │                                    │ │
│   │ ░░░░░░░░  │  │   │   Main Content Area                │ │
│   │ Host 3    │  │   │                                    │ │
│   │ ░░░░░░░░  │  │   │                                    │ │
│   └───────────┘  │   └────────────────────────────────────┘ │
│                  │                                          │
├──────────────────┴──────────────────────────────────────────┤
│  Status Bar: Ready | Hosts: 3 | Network: OK                 │
└─────────────────────────────────────────────────────────────┘
```

![Main Window Layout](screenshots/02_main_window.png)
*Screenshot: The main window layout with host list on the left and tabbed content on the right*

**Components:**
- **Host List Panel** (left): Displays all monitored hosts with status indicators and sparkline graphs
- **Tab Area** (right): Contains four tabs - Dashboard, Host Details, Interfaces, and Alerts
- **Splitter**: The divider between the host list and tab area can be dragged to resize

### Menu Bar

#### File Menu
| Action | Shortcut | Description |
|--------|----------|-------------|
| Add Host... | Ctrl+N | Add a new host to monitor |
| Remove Host | Delete | Remove the selected host |
| Edit Host... | - | Edit the selected host's settings |
| Export Data... | Ctrl+E | Export selected host's data to JSON/CSV |
| Quit | Ctrl+Q | Exit the application |

#### View Menu
| Action | Shortcut | Description |
|--------|----------|-------------|
| NOC Display Mode | F11 | Toggle full-screen NOC mode |

#### Tools Menu
| Action | Shortcut | Description |
|--------|----------|-------------|
| Port Scanner... | Ctrl+P | Open the port scanner dialog |
| Settings... | Ctrl+, | Open the settings dialog |

#### Help Menu
| Action | Description |
|--------|-------------|
| About NetPulse | Show version and credits |

### Toolbar

The toolbar provides quick access to common actions:

![Toolbar](screenshots/03_toolbar.png)
*Screenshot: The main toolbar with Add Host, Remove Host, Port Scan, and Settings buttons*

| Button | Description |
|--------|-------------|
| Add Host | Opens dialog to add a new monitored host |
| Remove Host | Removes the currently selected host (requires selection) |
| Port Scan | Opens the port scanner dialog |
| Settings | Opens the settings dialog |

### Status Bar

The status bar displays real-time information:

![Status Bar](screenshots/04_status_bar.png)
*Screenshot: The status bar showing status message, host count, and network status*

| Section | Description |
|---------|-------------|
| Status Message | Displays the most recent action or event |
| Host Count | Shows the total number of monitored hosts |
| Network Status | Indicates overall network connectivity |

---

## Host Management

### Adding a Host

To add a new host for monitoring:

1. Click **File > Add Host...** or press **Ctrl+N** or click the toolbar button
2. Enter the host name (a friendly label)
3. Click **OK**
4. Enter the host address (IP address or hostname)
5. Click **OK**

![Add Host Dialog - Name](screenshots/05_add_host_name.png)
*Screenshot: Dialog prompting for the host name*

![Add Host Dialog - Address](screenshots/06_add_host_address.png)
*Screenshot: Dialog prompting for the host address*

The new host will appear in the host list and monitoring will begin automatically.

**Tips:**
- Use descriptive names like "Web Server - Production" or "Router - Main Office"
- For IP addresses, both IPv4 (192.168.1.1) and IPv6 are supported
- Hostnames will be resolved via DNS

### Editing a Host

To edit an existing host:

1. Select the host in the host list
2. Click **File > Edit Host...** or double-click the host
3. Modify the host name
4. Click **OK**

![Edit Host Dialog](screenshots/07_edit_host.png)
*Screenshot: Dialog showing host name editing*

### Removing a Host

To remove a host:

1. Select the host in the host list
2. Click **File > Remove Host** or press **Delete**
3. Confirm the removal in the dialog

![Remove Host Confirmation](screenshots/08_remove_host.png)
*Screenshot: Confirmation dialog asking "Are you sure you want to remove 'hostname'?"*

**Warning:** Removing a host also deletes all associated historical data.

### Host Groups

Hosts can be organized into collapsible groups for better organization:

![Host Groups](screenshots/09_host_groups.png)
*Screenshot: Host list showing expanded and collapsed groups with multiple hosts*

Groups support:
- Drag-and-drop to reorder hosts
- Color coding for visual identification
- Collapsible sections to manage large host lists

---

## Dashboard

### Dashboard Overview

The Dashboard tab provides a customizable overview of your network status with multiple widgets arranged in a grid layout.

![Dashboard Overview](screenshots/10_dashboard_overview.png)
*Screenshot: The dashboard tab showing multiple widgets including statistics, host status, and alerts*

### Available Widgets

Access the widget toolbar at the top of the dashboard to add widgets:

![Widget Toolbar](screenshots/11_widget_toolbar.png)
*Screenshot: The widget toolbar showing available widget types*

| Widget | Description |
|--------|-------------|
| **Statistics** | Shows hosts up/down count, average latency, and packet loss percentage |
| **Host Status** | Summary view of all monitored hosts |
| **Alerts** | Filterable list of recent alerts |
| **Network Overview** | Network-wide health statistics |
| **Latency History** | Historical latency trends across all hosts |
| **Topology** | Interactive network topology visualization |

#### Statistics Widget

![Statistics Widget](screenshots/12_statistics_widget.png)
*Screenshot: Statistics widget showing Hosts Up: 5, Hosts Down: 1, Avg Latency: 45ms, Packet Loss: 2%*

The statistics widget displays:
- Number of hosts currently up and down
- Average latency across all monitored hosts
- Overall packet loss percentage

#### Host Status Widget

![Host Status Widget](screenshots/13_host_status_widget.png)
*Screenshot: Host status widget showing a summary grid of all hosts with status colors*

Provides a quick visual overview of all hosts with color-coded status indicators.

#### Alerts Widget

![Alerts Widget](screenshots/14_alerts_widget.png)
*Screenshot: Alerts widget with filter dropdowns and list of recent alerts*

Features:
- Severity filter (Info, Warning, Critical)
- Type filter (Host Down, High Latency, Packet Loss, etc.)
- Status filter (Active, Acknowledged)
- Full-text search

#### Topology Widget

![Topology Widget](screenshots/15_topology_widget.png)
*Screenshot: Network topology visualization showing hosts as nodes connected to a central hub*

The topology widget uses a force-directed layout to visualize your network:
- Central node represents your monitoring point
- Hosts appear as nodes colored by status (green/yellow/red)
- Labels show host name and current latency
- Supports zoom and pan navigation
- Click nodes to select hosts

### Customizing Layout

**Adding Widgets:**
1. Click the desired widget type in the toolbar
2. The widget appears in the next available grid position

**Removing Widgets:**
1. Right-click on a widget
2. Select "Remove Widget"

**Resetting Layout:**
1. Click the "Reset Layout" button in the widget toolbar
2. All widgets are removed; add them back as needed

Widget layouts are automatically saved and restored between sessions.

---

## Host Details

### Latency Chart

The Host Details tab shows detailed latency information for the selected host:

![Latency Chart](screenshots/16_latency_chart.png)
*Screenshot: Line chart showing latency over time with warning (yellow) and critical (red) threshold lines*

**Chart Features:**
- X-axis: Time progression
- Y-axis: Latency in milliseconds
- Yellow line: Warning threshold (default 100ms)
- Red line: Critical threshold (default 500ms)
- Up to 300 data points displayed
- Auto-scaling based on data range

**Selecting a Host:**
Click on any host in the host list to view its detailed latency chart.

### Sparkline Indicators

Each host in the host list displays a mini sparkline chart:

![Sparkline Indicators](screenshots/17_sparklines.png)
*Screenshot: Host list entries showing colored sparkline graphs next to each host name*

**Sparkline Colors:**
- **Green**: All measurements within normal range
- **Yellow**: Some measurements exceeded warning threshold
- **Red**: Some measurements exceeded critical threshold

Sparklines show the last 30 data points for quick trend identification.

---

## Port Scanner

### Running a Scan

The port scanner allows you to discover open ports on any host:

1. Open **Tools > Port Scanner...** or press **Ctrl+P**
2. Enter the target address
3. Select a port range preset or enter custom ports
4. Adjust concurrency and timeout settings
5. Click **Start**

![Port Scanner Dialog](screenshots/18_port_scanner.png)
*Screenshot: Port scanner dialog showing target input, port range dropdown, concurrency spinner, and results table*

**Port Range Options:**
| Preset | Ports |
|--------|-------|
| Common Ports | 20-25, 53, 80, 110, 143, 443, 993, 995, 3306, 3389, 5432, 8080 |
| Well-Known (0-1023) | All ports from 0 to 1023 |
| Registered (1024-49151) | All ports from 1024 to 49151 |
| Full Scan (0-65535) | All 65,536 ports |
| Custom | Enter specific ports or ranges |

**Settings:**
- **Concurrency**: Number of simultaneous port checks (default: 100)
- **Timeout**: Connection timeout in milliseconds (default: 1000ms)

### Interpreting Results

![Port Scan Results](screenshots/19_port_scan_results.png)
*Screenshot: Port scanner with completed scan showing table of open ports with port number, status, and service name*

The results table shows:
| Column | Description |
|--------|-------------|
| Port | Port number |
| Status | Open, Closed, or Filtered |
| Service | Detected service name (e.g., http, ssh, https) |

**Status Meanings:**
- **Open**: Port is accepting connections
- **Closed**: Port is reachable but not accepting connections
- **Filtered**: Port did not respond (possibly firewalled)

---

## Alerts

### Alert Types

NetPulse generates alerts based on configurable thresholds:

| Type | Severity | Trigger |
|------|----------|---------|
| Host Down | Critical | Host fails to respond |
| High Latency | Warning/Critical | Latency exceeds threshold |
| Packet Loss | Warning | Consecutive packet losses |
| Host Recovered | Info | Previously down host is back up |
| Scan Complete | Info | Port scan finished |

![Alert Types](screenshots/20_alert_types.png)
*Screenshot: Alerts tab showing different alert types with colored severity indicators*

### Managing Alerts

View alerts in the Alerts tab or the dashboard Alerts widget:

![Alert Management](screenshots/21_alert_management.png)
*Screenshot: Alerts panel with filter controls and alert list*

**Filtering Alerts:**
- By severity: Info, Warning, Critical
- By type: Host Down, High Latency, Packet Loss, Host Recovered
- By status: Active, Acknowledged
- By text: Full-text search on alert messages

**Acknowledging Alerts:**
Click on an alert to view details and acknowledge it.

---

## NOC Display Mode

### Entering NOC Mode

NOC (Network Operations Center) mode provides a full-screen display optimized for wall monitors:

1. Press **F11** or select **View > NOC Display Mode**
2. The interface switches to full-screen with large status cards

![NOC Mode Entry](screenshots/22_noc_mode_toggle.png)
*Screenshot: View menu with NOC Display Mode option highlighted*

### NOC Interface

![NOC Display](screenshots/23_noc_display.png)
*Screenshot: Full-screen NOC mode showing a 4-column grid of large host cards with status colors*

**NOC Mode Features:**
- 4-column grid layout for host cards
- Large, visible-from-distance status indicators
- Real-time clock display
- Summary statistics (hosts up/down/warning)
- System uptime tracking
- Auto-refreshing display

**Host Cards Display:**
- Host name (large text)
- Current status (color-coded background)
- Current latency value

**Exiting NOC Mode:**
- Press **Escape**
- Press **F11** again
- Double-click anywhere on the screen

---

## Settings

Access settings via **Tools > Settings...** or **Ctrl+,**

![Settings Dialog](screenshots/24_settings_dialog.png)
*Screenshot: Settings dialog showing tabbed sections for General, Monitoring, Alerts, and Data Retention*

### General Settings

| Setting | Description | Default |
|---------|-------------|---------|
| Theme | Visual theme (Light/Dark/System) | System |
| Start Minimized | Launch to system tray | Off |
| Minimize to Tray | Minimize to tray instead of taskbar | On |
| Start on Login | Auto-start with system | Off |

### Monitoring Settings

| Setting | Description | Default |
|---------|-------------|---------|
| Default Ping Interval | Seconds between pings | 30 |
| Warning Threshold | Latency for warning status (ms) | 100 |
| Critical Threshold | Latency for critical status (ms) | 500 |

### Alert Settings

| Setting | Description | Default |
|---------|-------------|---------|
| Latency Warning | Generate warning alert at (ms) | 100 |
| Latency Critical | Generate critical alert at (ms) | 500 |
| Consecutive Failures | Failures before host down alert | 3 |
| Desktop Notifications | Show system notifications | On |

### Data Retention

| Setting | Description | Default |
|---------|-------------|---------|
| Retention Days | Keep data for this many days | 30 |
| Auto Cleanup | Automatically delete old data | On |

---

## Data Export

Export host monitoring data for analysis:

1. Select a host in the host list
2. Click **File > Export Data...** or press **Ctrl+E**
3. Choose a file location and format

![Export Dialog](screenshots/25_export_dialog.png)
*Screenshot: File save dialog with JSON and CSV format options*

**Export Formats:**
- **JSON**: Full structured data with metadata
- **CSV**: Tabular format for spreadsheet import

**Exported Data Includes:**
- Host information
- All ping results with timestamps
- Latency measurements
- Success/failure status

---

## System Tray

NetPulse can run in the system tray for background monitoring:

![System Tray](screenshots/26_system_tray.png)
*Screenshot: System tray showing NetPulse icon with context menu*

**Tray Menu Options:**
- **Show**: Restore the main window
- **Hide**: Minimize to tray
- **Quit**: Exit the application

**Tray Behaviors:**
- Click: Toggle window visibility
- Right-click: Show context menu

When "Minimize to Tray" is enabled in settings, the close button minimizes to tray instead of quitting.

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | Add new host |
| Delete | Remove selected host |
| Ctrl+E | Export selected host data |
| Ctrl+P | Open port scanner |
| Ctrl+, | Open settings |
| Ctrl+Q | Quit application |
| F11 | Toggle NOC display mode |
| Escape | Exit NOC mode |

---

## Troubleshooting

### Common Issues

#### "Permission denied" when pinging (Linux)

ICMP ping requires special permissions on Linux:
```bash
sudo setcap cap_net_raw+ep ./build/netpulse
```

#### High latency readings on localhost

This is normal and expected. Localhost ping typically shows 0-1ms but may fluctuate due to system load.

#### Hosts show as "Down" despite being reachable

1. Verify the address is correct
2. Check if the host allows ICMP (ping) traffic
3. Ensure no firewall is blocking ICMP
4. Verify you have network connectivity

#### Database errors on startup

The database may be corrupted. Backup and remove:
```bash
mv ~/.local/share/NetPulse/NetPulse/netpulse.db netpulse.db.backup
```
NetPulse will create a fresh database on next launch.

#### Port scan shows all ports filtered

The target host may have a firewall blocking scan attempts. This is normal security behavior.

### Log Files

NetPulse logs are stored at:
```
~/.local/share/NetPulse/NetPulse/netpulse.log
```

View logs for debugging:
```bash
tail -f ~/.local/share/NetPulse/NetPulse/netpulse.log
```

### Getting Help

- GitHub Issues: https://github.com/00quasr/bws/issues
- Check the log file for error details
- Include NetPulse version and OS when reporting issues

---

## Appendix: Platform Notes

### Linux
- Requires `CAP_NET_RAW` capability for ICMP
- Uses `SOCK_RAW` for ping
- Data stored in `~/.local/share/NetPulse/`

### macOS
- Uses non-privileged `SOCK_DGRAM` for ICMP
- No special permissions required
- Data stored in `~/Library/Application Support/NetPulse/`

### Windows
- Uses `IcmpSendEcho2` API
- No special permissions required
- Data stored in `%APPDATA%/NetPulse/`

---

*NetPulse User Manual v1.0.0*
*© 2024 bws Project*
