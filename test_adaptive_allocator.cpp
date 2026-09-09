#include <cassert>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

#include "adaptive_allocator.hpp"

using namespace adaptive;

/**
 * @brief Tests basic functionality: element insertion, indexing access, and capacity bounds.
 */
void test_basic_usage() {
    std::cout << "[RUN] Test 1: Basic allocation and element access... ";

    adaptive_vector<int> vec;
    constexpr int kElementCount = 1000;

    for (int i = 0; i < kElementCount; ++i) {
        vec.push_back(i);
    }

    assert(vec.size() == kElementCount);
    assert(vec.capacity() >= kElementCount);

    for (int i = 0; i < kElementCount; ++i) {
        assert(vec[i] == i);
    }

    std::cout << "PASSED\n";
}

/**
 * @brief Verifies growth strategy adaptation when high-frequency insertions occur.
 */
void test_growth_strategy_switch() {
    std::cout << "[RUN] Test 2: Dynamic strategy adaptation under high load... ";

    // Custom telemetry tuned specifically for predictable test behavior
    auto telemetry = std::make_shared<allocation_telemetry>(
        /*shrink_idle_ms=*/1000,
        /*history_window_ms=*/50,
        /*high_threshold=*/500.0f);

    adaptive_vector<int> vec(telemetry);

    // Perform high-frequency insertions to trigger conservative strategy (>500 ops/sec).
    constexpr int kHighLoadCount = 5000;
    for (int i = 0; i < kHighLoadCount; ++i) {
        vec.push_back(i);
    }

    assert(vec.size() == kHighLoadCount);

    const double overhead = static_cast<double>(vec.capacity()) / vec.size();
    std::cout << "(Overhead Factor: " << overhead << ") ";

    // Under conservative strategy (1.1x factor), overhead factor must stay controlled (< 1.5).
    assert(overhead < 1.5);

    std::cout << "PASSED\n";
}

/**
 * @brief Verifies move semantics, container state integrity, and memory safety.
 */
void test_memory_safety() {
    std::cout << "[RUN] Test 3: Move semantics and lifetime correctness... ";

    adaptive_vector<int> vec1;
    constexpr int kSampleCount = 200;

    for (int i = 0; i < kSampleCount; ++i) {
        vec1.push_back(i);
    }

    // Test move assignment.
    adaptive_vector<int> vec2;
    vec2 = std::move(vec1);

    assert(vec2.size() == kSampleCount);
    assert(vec1.size() == 0);  // NOLINT: explicitly verifying moved-from container state

    for (int i = 0; i < kSampleCount; ++i) {
        assert(vec2[i] == i);
    }

    std::cout << "PASSED\n";
}

/**
 * @brief Tests high-frequency batch processing mimicking real-time order-book telemetry.
 */
void test_high_frequency_insertions() {
    std::cout << "[RUN] Test 4: High-frequency burst insertions... ";

    adaptive_vector<int> vec;
    constexpr int kBatches = 10;
    constexpr int kBatchSize = 1000;

    for (int batch = 0; batch < kBatches; ++batch) {
        for (int i = 0; i < kBatchSize; ++i) {
            vec.emplace_back(batch * kBatchSize + i);
        }
        // Short pause between transaction bursts
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    assert(vec.size() == kBatches * kBatchSize);

    const double overhead = static_cast<double>(vec.capacity()) / vec.size();
    std::cout << "(Burst Overhead: " << overhead << ") ";
    assert(overhead < 1.4);

    std::cout << "PASSED\n";
}

/**
 * @brief Tests multithreaded contention to verify shared telemetry thread safety.
 */
void test_multithreaded_contention() {
    std::cout << "[RUN] Test 5: Concurrent operations & telemetry safety... ";

    auto shared_telemetry = std::make_shared<allocation_telemetry>();
    constexpr int kNumThreads = 4;
    constexpr int kOpsPerThread = 2000;

    std::vector<std::future<void>> futures;
    futures.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [shared_telemetry, t]() {
            adaptive_vector<int> thread_local_vec(shared_telemetry);
            for (int i = 0; i < kOpsPerThread; ++i) {
                thread_local_vec.push_back(t * kOpsPerThread + i);
            }
            assert(thread_local_vec.size() == kOpsPerThread);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    std::cout << "PASSED\n";
}

int main() {
    std::cout << "   Adaptive Allocator & Vector Test Suite.      \n";

    try {
        test_basic_usage();
        test_growth_strategy_switch();
        test_memory_safety();
        test_high_frequency_insertions();
        test_multithreaded_contention();

        std::cout << "\n[SUCCESS] All test suites passed cleanly!\n";
    } catch (const std::exception& ex) {
        std::cerr << "\n[FAILURE] Exception caught: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
