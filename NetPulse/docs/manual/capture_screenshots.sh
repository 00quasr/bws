#!/bin/bash
# Screenshot capture script for NetPulse User Manual
# Run this script with a display available (X11 or Wayland)
#
# Prerequisites:
# - NetPulse built: cmake --build build
# - imagemagick installed: sudo apt install imagemagick
# - scrot or gnome-screenshot installed
# - ICMP capability: sudo setcap cap_net_raw+ep ./build/netpulse
#
# Usage:
#   ./capture_screenshots.sh
#
# This will launch NetPulse and guide you through capturing screenshots
# for the user manual.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCREENSHOTS_DIR="$SCRIPT_DIR/screenshots"
NETPULSE_BIN="$PROJECT_DIR/build/netpulse"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}NetPulse Screenshot Capture Script${NC}"
echo "======================================"
echo ""

# Check prerequisites
if [ ! -f "$NETPULSE_BIN" ]; then
    echo -e "${RED}Error: NetPulse binary not found at $NETPULSE_BIN${NC}"
    echo "Please build NetPulse first:"
    echo "  cd $PROJECT_DIR && cmake -B build && cmake --build build"
    exit 1
fi

if ! command -v import &> /dev/null && ! command -v scrot &> /dev/null; then
    echo -e "${YELLOW}Warning: Neither imagemagick (import) nor scrot found${NC}"
    echo "Install one of them:"
    echo "  sudo apt install imagemagick"
    echo "  # or"
    echo "  sudo apt install scrot"
fi

mkdir -p "$SCREENSHOTS_DIR"

echo -e "${GREEN}Screenshots will be saved to: $SCREENSHOTS_DIR${NC}"
echo ""

# Screenshot capture function
capture() {
    local name="$1"
    local description="$2"
    local filepath="$SCREENSHOTS_DIR/$name.png"

    echo -e "${YELLOW}Screenshot: $name${NC}"
    echo "  Description: $description"
    echo ""
    read -p "Press Enter when ready to capture, or 's' to skip: " response

    if [ "$response" = "s" ]; then
        echo "  Skipped."
        return
    fi

    if command -v scrot &> /dev/null; then
        scrot -s "$filepath"
    elif command -v import &> /dev/null; then
        import "$filepath"
    elif command -v gnome-screenshot &> /dev/null; then
        gnome-screenshot -a -f "$filepath"
    else
        echo -e "${RED}No screenshot tool available!${NC}"
        return 1
    fi

    echo -e "  ${GREEN}Saved: $filepath${NC}"
    echo ""
}

# Interactive screenshot session
echo -e "${BLUE}Starting interactive screenshot session${NC}"
echo "For each screenshot, the script will prompt you."
echo "Use your preferred method to select the window/region."
echo ""

echo "First, start NetPulse in a separate terminal:"
echo -e "  ${GREEN}$NETPULSE_BIN${NC}"
echo ""
read -p "Press Enter when NetPulse is running..."

echo ""
echo -e "${BLUE}=== Getting Started Screenshots ===${NC}"
capture "01_first_launch" "Main window on first launch with empty host list"
capture "02_main_window" "Main window with host list and tabbed content"

echo ""
echo -e "${BLUE}=== Menu and Toolbar Screenshots ===${NC}"
capture "03_toolbar" "Main toolbar close-up"
capture "04_status_bar" "Status bar close-up"

echo ""
echo -e "${BLUE}=== Host Management Screenshots ===${NC}"
echo "Add a test host (e.g., 8.8.8.8 or localhost) before these screenshots"
capture "05_add_host_name" "Add Host dialog - name prompt"
capture "06_add_host_address" "Add Host dialog - address prompt"
capture "07_edit_host" "Edit Host dialog"
capture "08_remove_host" "Remove Host confirmation dialog"
capture "09_host_groups" "Host list showing groups (if configured)"

echo ""
echo -e "${BLUE}=== Dashboard Screenshots ===${NC}"
capture "10_dashboard_overview" "Dashboard tab with multiple widgets"
capture "11_widget_toolbar" "Widget toolbar showing available widget types"
capture "12_statistics_widget" "Statistics widget close-up"
capture "13_host_status_widget" "Host Status widget close-up"
capture "14_alerts_widget" "Alerts widget with filters"
capture "15_topology_widget" "Network topology visualization"

echo ""
echo -e "${BLUE}=== Host Details Screenshots ===${NC}"
echo "Select a host and go to the Host Details tab"
capture "16_latency_chart" "Latency chart with threshold lines"
capture "17_sparklines" "Host list with sparkline indicators"

echo ""
echo -e "${BLUE}=== Port Scanner Screenshots ===${NC}"
echo "Open Tools > Port Scanner..."
capture "18_port_scanner" "Port scanner dialog before scan"
echo "Start a scan on localhost or a test host"
capture "19_port_scan_results" "Port scanner with completed results"

echo ""
echo -e "${BLUE}=== Alerts Screenshots ===${NC}"
capture "20_alert_types" "Alerts tab showing different alert types"
capture "21_alert_management" "Alerts panel with filters"

echo ""
echo -e "${BLUE}=== NOC Mode Screenshots ===${NC}"
capture "22_noc_mode_toggle" "View menu with NOC Display Mode"
echo "Press F11 to enter NOC mode"
capture "23_noc_display" "Full-screen NOC display"
echo "Press Escape to exit NOC mode"

echo ""
echo -e "${BLUE}=== Settings Screenshots ===${NC}"
echo "Open Tools > Settings..."
capture "24_settings_dialog" "Settings dialog"

echo ""
echo -e "${BLUE}=== Export and Tray Screenshots ===${NC}"
echo "Select a host and use File > Export Data..."
capture "25_export_dialog" "Export file dialog"
capture "26_system_tray" "System tray icon with context menu"

echo ""
echo -e "${GREEN}Screenshot capture complete!${NC}"
echo "Screenshots saved to: $SCREENSHOTS_DIR"
echo ""
echo "List of captured screenshots:"
ls -la "$SCREENSHOTS_DIR"/*.png 2>/dev/null || echo "No screenshots captured."
