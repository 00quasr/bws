cmake_minimum_required(VERSION 3.20)
project(NetPulse VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Network)

# Find other dependencies
find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sodium_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(sodium REQUIRED IMPORTED_TARGET libsodium)
endif()

# Find Asio (header-only)
find_path(ASIO_INCLUDE_DIR asio.hpp)
if(NOT ASIO_INCLUDE_DIR)
    message(STATUS "Asio not found via find_path, checking for package")
    find_package(asio CONFIG QUIET)
endif()

# Core types library (Qt-independent)
add_library(netpulse_core STATIC
    src/core/types/Host.cpp
    src/core/types/HostGroup.cpp
    src/core/types/PingResult.cpp
    src/core/types/PortScanResult.cpp
    src/core/types/NetworkInterface.cpp
    src/core/types/Alert.cpp
    src/core/types/ScheduledPortScan.cpp
    src/core/types/Notification.cpp
    src/core/types/SnmpTypes.cpp
)
target_include_directories(netpulse_core PUBLIC src)
target_link_libraries(netpulse_core PUBLIC nlohmann_json::nlohmann_json)
set_compiler_warnings(netpulse_core)

# Export plugin interface for external plugins
add_library(netpulse_plugin_interface INTERFACE)
target_include_directories(netpulse_plugin_interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(netpulse_plugin_interface INTERFACE netpulse_core)

# Infrastructure library
add_library(netpulse_infra STATIC
    src/infrastructure/network/AsioContext.cpp
    src/infrastructure/network/PingService.cpp
    src/infrastructure/network/PortScanner.cpp
    src/infrastructure/network/ScheduledPortScanner.cpp
    src/infrastructure/network/SnmpService.cpp
    src/infrastructure/database/Database.cpp
    src/infrastructure/database/HostRepository.cpp
    src/infrastructure/database/HostGroupRepository.cpp
    src/infrastructure/database/MetricsRepository.cpp
    src/infrastructure/database/ScheduledScanRepository.cpp
    src/infrastructure/database/SnmpRepository.cpp
    src/infrastructure/crypto/SecureStorage.cpp
    src/infrastructure/config/ConfigManager.cpp
    src/infrastructure/notifications/HttpClient.cpp
    src/infrastructure/notifications/NotificationService.cpp
    src/infrastructure/api/RestApiServer.cpp
    src/infrastructure/plugin/PluginManager.cpp
    src/infrastructure/plugin/PluginContext.cpp
)
target_include_directories(netpulse_infra PUBLIC src)
if(ASIO_INCLUDE_DIR)
    target_include_directories(netpulse_infra PUBLIC ${ASIO_INCLUDE_DIR})
endif()
target_compile_definitions(netpulse_infra PRIVATE ASIO_STANDALONE)
target_link_libraries(netpulse_infra PUBLIC
    netpulse_core
    SQLite::SQLite3
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Qt6::Core
    Qt6::Network
)
if(unofficial-sodium_FOUND)
    target_link_libraries(netpulse_infra PUBLIC unofficial-sodium::sodium)
else()
    target_link_libraries(netpulse_infra PUBLIC PkgConfig::sodium)
endif()

# Windows-specific libraries for ICMP ping
if(WIN32)
    target_link_libraries(netpulse_infra PUBLIC ws2_32 iphlpapi)
endif()

# Dynamic library loading
if(NOT WIN32)
    target_link_libraries(netpulse_infra PUBLIC ${CMAKE_DL_LIBS})
endif()

set_compiler_warnings(netpulse_infra)

# ViewModels library
add_library(netpulse_viewmodels STATIC
    src/viewmodels/DashboardViewModel.cpp
    src/viewmodels/HostMonitorViewModel.cpp
    src/viewmodels/HostGroupViewModel.cpp
    src/viewmodels/AlertsViewModel.cpp
    src/viewmodels/ScheduledScanViewModel.cpp
    src/viewmodels/SnmpMonitorViewModel.cpp
)
target_include_directories(netpulse_viewmodels PUBLIC src)
target_link_libraries(netpulse_viewmodels PUBLIC
    netpulse_infra
    Qt6::Core
)
set_compiler_warnings(netpulse_viewmodels)

# UI resources
qt_add_resources(UI_RESOURCES src/ui/resources/resources.qrc)

# Main application
add_executable(netpulse
    src/main.cpp
    src/app/Application.cpp
    src/app/SingleInstance.cpp
    src/ui/windows/MainWindow.cpp
    src/ui/windows/SettingsDialog.cpp
    src/ui/windows/PortScanDialog.cpp
    src/ui/widgets/LatencyChartWidget.cpp
    src/ui/widgets/HostListWidget.cpp
    src/ui/widgets/SparklineWidget.cpp
    src/ui/widgets/StatusIndicator.cpp
    src/ui/widgets/dashboard/DashboardWidget.cpp
    src/ui/widgets/dashboard/DashboardContainer.cpp
    src/ui/widgets/dashboard/WidgetFactory.cpp
    src/ui/widgets/dashboard/WidgetToolbar.cpp
    src/ui/widgets/dashboard/StatisticsWidget.cpp
    src/ui/widgets/dashboard/HostStatusWidget.cpp
    src/ui/widgets/dashboard/AlertsWidget.cpp
    src/ui/widgets/dashboard/NetworkOverviewWidget.cpp
    src/ui/widgets/dashboard/LatencyHistoryWidget.cpp
    src/ui/widgets/dashboard/TopologyWidget.cpp
    src/ui/widgets/noc/NocHostCard.cpp
    src/ui/widgets/noc/NocDisplayWidget.cpp
    ${UI_RESOURCES}
)
target_include_directories(netpulse PRIVATE src)
target_link_libraries(netpulse PRIVATE
    netpulse_viewmodels
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network
)
set_compiler_warnings(netpulse)

# Testing
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        find_package(Catch2 2 REQUIRED)
    endif()

    add_executable(netpulse_tests
        tests/unit/test_Host.cpp
        tests/unit/test_HostGroup.cpp
        tests/unit/test_HostRepository.cpp
        tests/unit/test_MetricsRepository.cpp
        tests/unit/test_Database.cpp
        tests/unit/test_PingService.cpp
        tests/unit/test_SparklineWidget.cpp
        tests/unit/test_DashboardWidget.cpp
        tests/unit/test_NocHostCard.cpp
        tests/unit/test_AlertFilter.cpp
        tests/unit/test_TopologyWidget.cpp
        tests/unit/test_ScheduledPortScan.cpp
        tests/unit/test_NotificationService.cpp
        tests/unit/test_RestApiServer.cpp
        tests/unit/test_SnmpService.cpp
        tests/unit/test_PluginSystem.cpp
        tests/unit/test_ConfigManager.cpp
        tests/unit/test_AlertsViewModel.cpp
        tests/unit/test_Alert.cpp
        tests/unit/test_PingResult.cpp
        tests/unit/test_PortScanResult.cpp
        tests/unit/test_Notification.cpp
        tests/unit/test_SnmpTypes.cpp
        tests/integration/test_host_monitoring_lifecycle.cpp
        tests/integration/test_alert_generation_workflow.cpp
        tests/integration/test_multi_host_monitoring.cpp
        tests/integration/test_data_retention.cpp
        src/ui/widgets/dashboard/DashboardWidget.cpp
        src/ui/widgets/noc/NocHostCard.cpp
    )
    target_link_libraries(netpulse_tests PRIVATE
        netpulse_viewmodels
        Qt6::Widgets
        Qt6::Network
        Catch2::Catch2WithMain
    )

    include(CTest)
    include(Catch)
    catch_discover_tests(netpulse_tests)
endif()

# Example plugin (optional)
option(BUILD_EXAMPLE_PLUGIN "Build example plugin" OFF)
if(BUILD_EXAMPLE_PLUGIN)
    add_library(example_plugin SHARED
        plugins/example/ExamplePlugin.cpp
    )
    target_link_libraries(example_plugin PRIVATE
        netpulse_plugin_interface
    )
    set_target_properties(example_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "netpulse_example_plugin"
    )
endif()

# Installation
install(TARGETS netpulse RUNTIME DESTINATION bin)
install(DIRECTORY src/core/plugin/ DESTINATION include/netpulse/plugin FILES_MATCHING PATTERN "*.hpp")
