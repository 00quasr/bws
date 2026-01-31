cmake_minimum_required(VERSION 3.20)
project(ExamplePlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find NetPulse plugin interface
# When building as standalone, set NETPULSE_DIR to NetPulse source directory
if(NOT TARGET netpulse_plugin_interface)
    if(DEFINED NETPULSE_DIR)
        add_subdirectory(${NETPULSE_DIR} ${CMAKE_BINARY_DIR}/netpulse_deps EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "netpulse_plugin_interface target not found. Set NETPULSE_DIR.")
    endif()
endif()

# Build the plugin as a shared library
add_library(example_plugin SHARED
    ExamplePlugin.cpp
)

target_link_libraries(example_plugin PRIVATE
    netpulse_plugin_interface
)

# Set output name without 'lib' prefix on Unix
set_target_properties(example_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "netpulse_example_plugin"
)

# Installation
install(TARGETS example_plugin
    LIBRARY DESTINATION lib/netpulse/plugins
    RUNTIME DESTINATION lib/netpulse/plugins
)

# Install manifest file
install(FILES example_plugin.json
    DESTINATION lib/netpulse/plugins
    RENAME netpulse_example_plugin.json
)
