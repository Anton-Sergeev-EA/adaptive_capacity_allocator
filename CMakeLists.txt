cmake_minimum_required(VERSION 3.14)
project(AdaptiveAllocator 
    VERSION 1.0.0 
    DESCRIPTION "Production-grade high-performance dynamic adaptive allocator for C++17/20"
    LANGUAGES CXX
)

# Standard C++ Configuration.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Options.
option(ADAPTIVE_BUILD_TESTS "Build unit tests suite" ON)
option(ADAPTIVE_BUILD_BENCHMARKS "Build performance benchmarks" ON)

# ASan and TSan instrument incompatible things (shadow memory vs. happens-before
# tracking) and cannot be linked into the same binary - GCC/Clang both hard-error
# on -fsanitize=address,thread. Since this library's whole point is concurrent
# telemetry (a background worker thread plus a shared_ptr<allocation_telemetry>
# used across threads), both sanitizers are genuinely useful here, just not at
# the same time, so this is a tri-state choice rather than one on/off switch.
set(ADAPTIVE_SANITIZER "none" CACHE STRING "Sanitizer build: none, address (ASan+UBSan), or thread (TSan+UBSan)")
set_property(CACHE ADAPTIVE_SANITIZER PROPERTY STRINGS none address thread)

# Required Dependencies.
find_package(Threads REQUIRED)

# 1. Header-only Library Definition.
add_library(adaptive_allocator INTERFACE)
add_library(adaptive::allocator ALIAS adaptive_allocator)

target_include_directories(adaptive_allocator INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(adaptive_allocator INTERFACE
    Threads::Threads
)

# Compile options for consumers of the interface library.
if(MSVC)
    target_compile_options(adaptive_allocator INTERFACE /W4 /permissive-)
else()
    target_compile_options(adaptive_allocator INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Helper macro for compiler-specific warning options.
function(apply_strict_compile_options TARGET_NAME OPTIMIZATION_LEVEL)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX /${OPTIMIZATION_LEVEL})
    else()
        target_compile_options(${TARGET_NAME} PRIVATE 
            -Wall 
            -Wextra 
            -Wpedantic 
            -Werror 
            -${OPTIMIZATION_LEVEL}
        )
    endif()

    if(NOT MSVC AND NOT ADAPTIVE_SANITIZER STREQUAL "none")
        if(ADAPTIVE_SANITIZER STREQUAL "address")
            set(_adaptive_sanitize_flags -fsanitize=address,undefined)
        elseif(ADAPTIVE_SANITIZER STREQUAL "thread")
            # UBSan deliberately left out here: GCC's UBSan runtime calls
            # __sanitizer::IsAccessibleMemoryRange() (a pipe()-based pointer
            # probe) in a way that is not coordinated with ThreadSanitizer's
            # own interceptors, producing races reported entirely inside
            # libtsan/libubsan/libstdc++ - not in this project's code - when
            # both are loaded together. UBSan coverage is still exercised via
            # the "address" sanitizer mode above.
            set(_adaptive_sanitize_flags -fsanitize=thread)
        else()
            message(FATAL_ERROR "ADAPTIVE_SANITIZER must be one of: none, address, thread (got '${ADAPTIVE_SANITIZER}')")
        endif()
        target_compile_options(${TARGET_NAME} PRIVATE ${_adaptive_sanitize_flags} -g)
        target_link_options(${TARGET_NAME} PRIVATE ${_adaptive_sanitize_flags})
    endif()
endfunction()

# 2. Unit Tests Target.
if(ADAPTIVE_BUILD_TESTS)
    enable_testing()

    add_executable(adaptive_tests test_adaptive_allocator.cpp)
    target_link_libraries(adaptive_tests PRIVATE adaptive::allocator)
    apply_strict_compile_options(adaptive_tests "O2")

    add_test(NAME AdaptiveAllocatorTests COMMAND adaptive_tests)
endif()

# 3. Microbenchmarks Target.
if(ADAPTIVE_BUILD_BENCHMARKS)
    add_executable(adaptive_benchmark benchmark.cpp)
    target_link_libraries(adaptive_benchmark PRIVATE adaptive::allocator)
    apply_strict_compile_options(adaptive_benchmark "O3")
endif()
