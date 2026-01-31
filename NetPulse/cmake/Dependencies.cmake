# Dependencies.cmake
# Handles finding and configuring external dependencies

# Set paths for vcpkg if available
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Prefer config mode for finding packages
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
