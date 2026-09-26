cmake_minimum_required(VERSION 3.14)
project(AdaptiveAllocator
    VERSION 2.0.0
    DESCRIPTION "Telemetry-driven adaptive capacity growth for C++17/20 containers"
    HOMEPAGE_URL "https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator"
    LANGUAGES CXX
)

# Build extras only when this is the top-level project (not when pulled in via FetchContent).
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(ADAPTIVE_IS_TOP_LEVEL ON)
else()
    set(ADAPTIVE_IS_TOP_LEVEL OFF)
endif()

option(ADAPTIVE_BUILD_TESTS "Build the unit tests" ${ADAPTIVE_IS_TOP_LEVEL})

# ASan and TSan cannot be linked into one binary, so this is a three-way choice.
set(ADAPTIVE_SANITIZER "none" CACHE STRING "Sanitizer build: none, address (ASan+UBSan) or thread (TSan)")
set_property(CACHE ADAPTIVE_SANITIZER PROPERTY STRINGS none address thread)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES AND ADAPTIVE_IS_TOP_LEVEL)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Header-only library: adaptive::allocator
# ---------------------------------------------------------------------------
add_library(adaptive_allocator INTERFACE)
add_library(adaptive::allocator ALIAS adaptive_allocator)
set_target_properties(adaptive_allocator PROPERTIES EXPORT_NAME allocator)
target_compile_features(adaptive_allocator INTERFACE cxx_std_17)
target_include_directories(adaptive_allocator INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(adaptive_allocator INTERFACE Threads::Threads)

function(adaptive_strict_target target)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_features(${target} PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
    endif()
    if(NOT MSVC AND NOT ADAPTIVE_SANITIZER STREQUAL "none")
        if(ADAPTIVE_SANITIZER STREQUAL "address")
            set(flags -fsanitize=address,undefined -fno-omit-frame-pointer)
        elseif(ADAPTIVE_SANITIZER STREQUAL "thread")
            # UBSan is left out of TSan builds: GCC's UBSan runtime produces races inside the
            # sanitizer runtimes themselves when combined with TSan. UBSan runs in "address" mode.
            set(flags -fsanitize=thread)
        else()
            message(FATAL_ERROR "ADAPTIVE_SANITIZER must be none, address or thread (got '${ADAPTIVE_SANITIZER}')")
        endif()
        target_compile_options(${target} PRIVATE ${flags} -g)
        target_link_options(${target} PRIVATE ${flags})
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ADAPTIVE_BUILD_TESTS)
    enable_testing()
    add_executable(adaptive_tests tests/test_adaptive_allocator.cpp)
    target_link_libraries(adaptive_tests PRIVATE adaptive::allocator)
    adaptive_strict_target(adaptive_tests)
    add_test(NAME adaptive_tests COMMAND adaptive_tests)
endif()

