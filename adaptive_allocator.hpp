#pragma once

#include <cstddef>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <cassert>
#include <new>

namespace adaptive {

/**
 * @brief Strategy defining allocation aggressiveness.
 */
enum class growth_strategy {
    aggressive,   // 2.0x growth factor (for burst/rare inserts).
    moderate,     // 1.5x growth factor (balanced).
    conservative  // 1.1x growth factor (memory-constrained / high-frequency).
};

/**
 * @brief Shared telemetry and state manager for adaptive allocation decisions.
 * Decouples thread management and metrics from the C++ Allocator value semantics.
 */
class allocation_telemetry {
public:
    explicit allocation_telemetry(
        std::size_t shrink_idle_ms = 5000,
        std::size_t history_window_ms = 100,
        float high_threshold = 1000.0f
    ) : m_shrink_idle_ms(shrink_idle_ms),
        m_history_window_ms(history_window_ms),
        m_high_threshold(high_threshold),
        m_last_alloc_ticks(now_ticks()),
        m_last_shrink_ticks(now_ticks()),
        m_push_counter(0),
        m_stop_worker(false) 
    {
        m_worker_thread = std::thread(&allocation_telemetry::shrink_worker, this);
    }

    ~allocation_telemetry() {
        stop_worker();
    }

    // Non-copyable, non-movable to maintain thread stability.
    allocation_telemetry(const allocation_telemetry&) = delete;
    allocation_telemetry& operator=(const allocation_telemetry&) = delete;

    void record_allocation() noexcept {
        m_last_alloc_ticks.store(now_ticks(), std::memory_order_release);
        m_push_counter.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] growth_strategy calculate_strategy() const noexcept {
        const auto now = now_ticks();
        const auto last_alloc = m_last_alloc_ticks.load(std::memory_order_acquire);
        
        // If idle for over 1 second, revert to aggressive to minimize reallocation frequency.
        if ((now - last_alloc) > 1000) {
            return growth_strategy::aggressive;
        }

        const auto pushes = m_push_counter.load(std::memory_order_acquire);
        const float window_sec = static_cast<float>(m_history_window_ms) / 1000.0f;
        const float rate = static_cast<float>(pushes) / (window_sec > 0.0f ? window_sec : 1.0f);

        if (rate > m_high_threshold) {
            return growth_strategy::conservative;
        }
        if (rate > 100.0f) {
            return growth_strategy::moderate;
        }

        return growth_strategy::aggressive;
    }

    [[nodiscard]] std::size_t compute_capacity(std::size_t current_cap, std::size_t required_cap) const noexcept {
        const growth_strategy strategy = calculate_strategy();
        std::size_t new_cap = current_cap;

        switch (strategy) {
            case growth_strategy::aggressive:
                new_cap = current_cap + (current_cap >> 1) + (current_cap >> 2); // ~1.75x - 2.0x boost.
                if (new_cap < current_cap * 2) new_cap = current_cap * 2;
                break;
            case growth_strategy::moderate:
                new_cap = current_cap + (current_cap >> 1); // 1.5x.
                break;
            case growth_strategy::conservative:
                new_cap = current_cap + (current_cap / 10); // 1.1x.
                break;
        }

        constexpr std::size_t kMinCapacity = 8;
        return std::max({new_cap, required_cap, kMinCapacity});
    }

private:
    [[nodiscard]] static std::int64_t now_ticks() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    void shrink_worker() {
        std::unique_lock<std::mutex> lock(m_worker_mtx);
        while (!m_stop_worker) {
            // Efficient waiting using condition variable instead of hard thread sleep.
            if (m_cv.wait_for(lock, std::chrono::seconds(1), [this] { return m_stop_worker.load(); })) {
                break;
            }

            const auto now = now_ticks();
            const auto last_alloc = m_last_alloc_ticks.load(std::memory_order_acquire);
            const auto last_shrink = m_last_shrink_ticks.load(std::memory_order_acquire);

            const auto idle_ms = now - last_alloc;
            const auto since_shrink_ms = now - last_shrink;

            if (idle_ms > static_cast<std::int64_t>(m_shrink_idle_ms) &&
                since_shrink_ms > static_cast<std::int64_t>(m_shrink_idle_ms) / 2) {
                
                m_last_shrink_ticks.store(now, std::memory_order_release);
                m_push_counter.store(0, std::memory_order_relaxed);
            }
        }
    }

    void stop_worker() {
        {
            std::lock_guard<std::mutex> lock(m_worker_mtx);
            m_stop_worker = true;
        }
        m_cv.notify_all();
        if (m_worker_thread.joinable()) {
            m_worker_thread.join();
        }
    }

    std::size_t m_shrink_idle_ms;
    std::size_t m_history_window_ms;
    float m_high_threshold;

    std::atomic<std::int64_t> m_last_alloc_ticks;
    std::atomic<std::int64_t> m_last_shrink_ticks;
    std::atomic<std::size_t> m_push_counter;

    std::atomic<bool> m_stop_worker;
    std::mutex m_worker_mtx;
    std::condition_variable m_cv;
    std::thread m_worker_thread;
};

/**
 * @brief High-performance C++17/C++20 compliant stateless allocator.
 * Handles over-aligned types safely and tracks metrics via shared telemetry.
 */
template <typename T>
class adaptive_allocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    adaptive_allocator() noexcept = default;

    explicit adaptive_allocator(std::shared_ptr<allocation_telemetry> telemetry) noexcept
        : m_telemetry(std::move(telemetry)) {}

    template <typename U>
    adaptive_allocator(const adaptive_allocator<U>& other) noexcept
        : m_telemetry(other.m_telemetry) {}

    [[nodiscard]] T* allocate(size_type n) {
        if (n == 0) return nullptr;

        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_array_new_length();
        }

        if (m_telemetry) {
            m_telemetry->record_allocation();
        }

        std::size_t bytes = n * sizeof(T);

        // Correct support for modern C++ over-aligned types (e.g., AVX-512 / SIMD structures)
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            return static_cast<T*>(::operator new(bytes, std::align_val_t(alignof(T))));
        } else {
            return static_cast<T*>(::operator new(bytes));
        }
    }

    void deallocate(T* ptr, size_type n) noexcept {
        if (!ptr) return;

        std::size_t bytes = n * sizeof(T);
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            ::operator delete(ptr, bytes, std::align_val_t(alignof(T)));
        } else {
            ::operator delete(ptr, bytes);
        }
    }

    [[nodiscard]] std::shared_ptr<allocation_telemetry> get_telemetry() const noexcept {
        return m_telemetry;
    }

    template <typename U>
    friend bool operator==(const adaptive_allocator<T>& lhs, const adaptive_allocator<U>& rhs) noexcept {
        return lhs.m_telemetry == rhs.m_telemetry;
    }

    template <typename U>
    friend bool operator!=(const adaptive_allocator<T>& lhs, const adaptive_allocator<U>& rhs) noexcept {
        return !(lhs == rhs);
    }

private:
    template <typename U> friend class adaptive_allocator;
    std::shared_ptr<allocation_telemetry> m_telemetry{nullptr};
};

/**
 * @brief Custom Vector Wrapper that integrates adaptive telemetry-driven reallocation.
 * Since standard std::vector controls its own growth policy, this wrapper overrides
 * capacity allocation dynamically based on runtime metrics.
 */
template <typename T>
class adaptive_vector {
public:
    using allocator_type = adaptive_allocator<T>;
    using size_type = std::size_t;

    adaptive_vector() 
        : m_telemetry(std::make_shared<allocation_telemetry>()),
          m_alloc(m_telemetry) {}

    explicit adaptive_vector(std::shared_ptr<allocation_telemetry> telemetry)
        : m_telemetry(std::move(telemetry)),
          m_alloc(m_telemetry) {}

    void push_back(const T& value) {
        ensure_capacity(m_data.size() + 1);
        m_data.push_back(value);
    }

    void push_back(T&& value) {
        ensure_capacity(m_data.size() + 1);
        m_data.push_back(std::move(value));
    }

    template <typename... Args>
    decltype(auto) emplace_back(Args&&... args) {
        ensure_capacity(m_data.size() + 1);
        return m_data.emplace_back(std::forward<Args>(args)...);
    }

    void reserve(size_type new_cap) {
        m_data.reserve(new_cap);
    }

    void shrink_to_fit() {
        m_data.shrink_to_fit();
    }

    [[nodiscard]] size_type capacity() const noexcept { return m_data.capacity(); }
    [[nodiscard]] size_type size() const noexcept { return m_data.size(); }
    [[nodiscard]] const std::vector<T, allocator_type>& native() const noexcept { return m_data; }

private:
    void ensure_capacity(size_type min_required) {
        if (min_required > m_data.capacity()) {
            std::size_t next_cap = m_telemetry->compute_capacity(m_data.capacity(), min_required);
            m_data.reserve(next_cap);
        }
    }

    std::shared_ptr<allocation_telemetry> m_telemetry;
    allocator_type m_alloc;
    std::vector<T, allocator_type> m_data;
};

}
