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
option(ADAPTIVE_ENABLE_SANITIZERS "Enable Address and Thread Sanitizers for debugging" OFF)

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

    if(ADAPTIVE_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=address,thread,undefined -g)
        target_link_options(${TARGET_NAME} PRIVATE -fsanitize=address,thread,undefined)
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
