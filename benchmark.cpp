#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "adaptive_allocator.hpp"

using namespace adaptive;

/**
 * @brief Prevents compiler from optimizing away unused variables in microbenchmarks.
 */
template <typename T>
void volatile_sink(T&& val) {
    volatile auto sink = std::forward<T>(val);
    (void)sink;
}

/**
 * @brief Benchmarks vector insertion performance and memory efficiency overhead factor.
 */
template <typename Container>
void benchmark_insertions(const std::string& name, int elements) {
    // Warm-up run to eliminate cold-cache effect and initial OS page faults.
    {
        Container warmup_vec;
        for (int i = 0; i < 1000; ++i) {
            warmup_vec.push_back(i);
        }
        volatile_sink(warmup_vec.size());
    }

    Container vec;

    const auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < elements; ++i) {
        vec.push_back(i);
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    const double overhead = static_cast<double>(vec.capacity()) / vec.size();

    std::cout << std::left << std::setw(22) << name << "| Execution Time: " << std::setw(8)
              << (static_cast<double>(elapsed) / 1000.0) << " ms " << "| Memory Overhead (Cap/Size): " << std::fixed
              << std::setprecision(3) << overhead << "| Final Capacity: " << vec.capacity() << std::endl;
}

/**
 * @brief Benchmarks burst insertions to simulate high-frequency trading (HFT) / telemetry workloads.
 */
template <typename Container>
void benchmark_burst_load(const std::string& name, int total_elements, int burst_size) {
    Container vec;

    const auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total_elements; ++i) {
        vec.push_back(i);
        if (i % burst_size == 0) {
            // Micro-pause using yield instead of sleep to prevent OS scheduler distortion.
            std::this_thread::yield();
        }
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    const double overhead = static_cast<double>(vec.capacity()) / vec.size();

    std::cout << std::left << std::setw(22) << (name + " (Burst)") << "| Execution Time: " << std::setw(8)
              << (static_cast<double>(elapsed) / 1000.0) << " ms " << "| Memory Overhead (Cap/Size): " << std::fixed
              << std::setprecision(3) << overhead << " | Final Capacity: " << vec.capacity() << std::endl;
}

int main() {
    constexpr int kElements = 1'000'000;
    constexpr int kBurstSize = 5'000;

    std::cout << "ADAPTIVE ALLOCATOR BENCHMARK SUITE (High-Performance C++)\n";

    std::cout << "Test Scenario 1: Continuous High-Volume Insertion (1M elements)\n";
    benchmark_insertions<std::vector<int>>("std::vector", kElements);
    benchmark_insertions<adaptive_vector<int>>("adaptive_vector", kElements);

    std::cout << "\nTest Scenario 2: Burst / Intermittent Insertion (1M elements)\n";
    benchmark_burst_load<std::vector<int>>("std::vector", kElements, kBurstSize);
    benchmark_burst_load<adaptive_vector<int>>("adaptive_vector", kElements, kBurstSize);

    return 0;
}
