# Sanitizers.cmake
# Configures AddressSanitizer and other sanitizers for memory leak detection

option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer for race detection" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer for uninitialized memory detection" OFF)

# Validate incompatible sanitizer combinations
if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer cannot be used together")
endif()

if(ENABLE_ASAN AND ENABLE_MSAN)
    message(FATAL_ERROR "AddressSanitizer and MemorySanitizer cannot be used together")
endif()

if(ENABLE_TSAN AND ENABLE_MSAN)
    message(FATAL_ERROR "ThreadSanitizer and MemorySanitizer cannot be used together")
endif()

# Function to enable sanitizers on a target
function(enable_sanitizers target)
    if(NOT (ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN OR ENABLE_MSAN))
        return()
    endif()

    if(MSVC)
        if(ENABLE_ASAN)
            target_compile_options(${target} PRIVATE /fsanitize=address)
            target_link_options(${target} PRIVATE /fsanitize=address)
        endif()
        if(ENABLE_UBSAN OR ENABLE_TSAN OR ENABLE_MSAN)
            message(WARNING "UBSan, TSan, and MSan are not supported on MSVC")
        endif()
    else()
        set(SANITIZER_FLAGS "")
        set(SANITIZER_LINK_FLAGS "")

        if(ENABLE_ASAN)
            list(APPEND SANITIZER_FLAGS -fsanitize=address)
            list(APPEND SANITIZER_FLAGS -fno-omit-frame-pointer)
            list(APPEND SANITIZER_FLAGS -fno-optimize-sibling-calls)
            list(APPEND SANITIZER_LINK_FLAGS -fsanitize=address)
            # Enable leak sanitizer (part of ASan on Linux)
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                list(APPEND SANITIZER_FLAGS -fsanitize=leak)
                list(APPEND SANITIZER_LINK_FLAGS -fsanitize=leak)
            endif()
        endif()

        if(ENABLE_UBSAN)
            list(APPEND SANITIZER_FLAGS -fsanitize=undefined)
            list(APPEND SANITIZER_FLAGS -fno-omit-frame-pointer)
            list(APPEND SANITIZER_LINK_FLAGS -fsanitize=undefined)
        endif()

        if(ENABLE_TSAN)
            list(APPEND SANITIZER_FLAGS -fsanitize=thread)
            list(APPEND SANITIZER_FLAGS -fno-omit-frame-pointer)
            list(APPEND SANITIZER_LINK_FLAGS -fsanitize=thread)
        endif()

        if(ENABLE_MSAN)
            if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                list(APPEND SANITIZER_FLAGS -fsanitize=memory)
                list(APPEND SANITIZER_FLAGS -fno-omit-frame-pointer)
                list(APPEND SANITIZER_FLAGS -fsanitize-memory-track-origins)
                list(APPEND SANITIZER_LINK_FLAGS -fsanitize=memory)
            else()
                message(WARNING "MemorySanitizer is only supported with Clang")
            endif()
        endif()

        if(SANITIZER_FLAGS)
            target_compile_options(${target} PRIVATE ${SANITIZER_FLAGS})
            target_link_options(${target} PRIVATE ${SANITIZER_LINK_FLAGS})
        endif()
    endif()
endfunction()

# Print sanitizer status
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer: ENABLED")
    message(STATUS "  - To run with ASan, set ASAN_OPTIONS environment variable")
    message(STATUS "  - Example: ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 ./netpulse")
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer: ENABLED")
endif()

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer: ENABLED")
endif()

if(ENABLE_MSAN)
    message(STATUS "MemorySanitizer: ENABLED")
endif()
