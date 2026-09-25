#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

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
    explicit allocation_telemetry(std::size_t shrink_idle_ms = 5000,
                                  std::size_t history_window_ms = 100,
                                  float high_threshold = 1000.0f)
        : m_shrink_idle_ms(shrink_idle_ms),
          m_history_window_ms(history_window_ms),
          m_high_threshold(high_threshold),
          m_last_insert_ticks(now_ticks()),
          m_last_shrink_ticks(now_ticks()),
          m_window_start_ticks(now_ticks()),
          m_window_count(0),
          m_stop_worker(false) {
        m_worker_thread = std::thread(&allocation_telemetry::shrink_worker, this);
    }

    ~allocation_telemetry() { stop_worker(); }

    // Non-copyable, non-movable to maintain thread stability.
    allocation_telemetry(const allocation_telemetry&) = delete;
    allocation_telemetry& operator=(const allocation_telemetry&) = delete;

    /**
     * @brief Records one logical insertion (call once per push_back/emplace_back).
     *
     * This intentionally does NOT fire from the allocator's allocate(): that is
     * only invoked on an actual reallocation, which happens O(log n) times for
     * n insertions - counting those as "insertions" made the rate metric under-
     * count by orders of magnitude and left calculate_strategy() unable to ever
     * observe a realistic insertion rate. Recording here, once per element,
     * measures what the class actually claims to measure.
     *
     * Only resets the window when it has actually elapsed, so the counter
     * doesn't grow without bound over the container's whole lifetime. The rate
     * itself is computed on demand in calculate_strategy() from however much of
     * the current (possibly still-open) window has elapsed - see the comment
     * there for why it isn't computed here instead.
     */
    void record_insertion() noexcept {
        const auto now = now_ticks();
        m_last_insert_ticks.store(now, std::memory_order_release);

        const auto window_start = m_window_start_ticks.load(std::memory_order_relaxed);
        if ((now - window_start) >= static_cast<std::int64_t>(m_history_window_ms)) {
            m_window_start_ticks.store(now, std::memory_order_relaxed);
            m_window_count.store(1, std::memory_order_relaxed);
        } else {
            m_window_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] growth_strategy calculate_strategy() const noexcept {
        const auto now = now_ticks();
        const auto last_insert = m_last_insert_ticks.load(std::memory_order_acquire);

        // If idle for over 1 second, revert to aggressive to minimize reallocation frequency.
        if ((now - last_insert) > 1000) {
            return growth_strategy::aggressive;
        }

        // Rate is deliberately computed here, from the live (count, elapsed) pair,
        // rather than cached from the last completed window: a burst of inserts
        // that finishes before a single history_window_ms has even elapsed (the
        // common case for anything faster than ~10k ops/sec with the default
        // 100ms window) must still be visible immediately, not only once that
        // window eventually rolls over. Elapsed is floored to 1ms so a rate
        // sample taken a moment after record_insertion() rolls a fresh window
        // doesn't divide by (near) zero.
        const auto window_start = m_window_start_ticks.load(std::memory_order_acquire);
        const auto count = m_window_count.load(std::memory_order_acquire);
        const auto elapsed_ms = std::max<std::int64_t>(now - window_start, 1);
        const float rate = static_cast<float>(count) / (static_cast<float>(elapsed_ms) / 1000.0f);

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
                // Matches std::vector's own default growth factor (>= 2.0x).
                new_cap = current_cap * 2;
                break;
            case growth_strategy::moderate:
                new_cap = current_cap + (current_cap >> 1);  // 1.5x.
                break;
            case growth_strategy::conservative:
                new_cap = current_cap + (current_cap / 10);  // 1.1x.
                break;
        }

        constexpr std::size_t kMinCapacity = 8;
        return std::max({new_cap, required_cap, kMinCapacity});
    }

  private:
    [[nodiscard]] static std::int64_t now_ticks() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    void shrink_worker() {
        std::unique_lock<std::mutex> lock(m_worker_mtx);
        while (!m_stop_worker) {
            // Efficient waiting using condition variable instead of hard thread sleep.
            if (m_cv.wait_for(lock, std::chrono::seconds(1), [this] { return m_stop_worker.load(); })) {
                break;
            }

            const auto now = now_ticks();
            const auto last_insert = m_last_insert_ticks.load(std::memory_order_acquire);
            const auto last_shrink = m_last_shrink_ticks.load(std::memory_order_acquire);

            const auto idle_ms = now - last_insert;
            const auto since_shrink_ms = now - last_shrink;

            if (idle_ms > static_cast<std::int64_t>(m_shrink_idle_ms) &&
                since_shrink_ms > static_cast<std::int64_t>(m_shrink_idle_ms) / 2) {
                m_last_shrink_ticks.store(now, std::memory_order_release);
                m_window_count.store(0, std::memory_order_relaxed);
                m_window_start_ticks.store(now, std::memory_order_relaxed);
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

    std::atomic<std::int64_t> m_last_insert_ticks;
    std::atomic<std::int64_t> m_last_shrink_ticks;

    // Rolling-window insertion-rate tracking (see record_insertion()).
    std::atomic<std::int64_t> m_window_start_ticks;
    std::atomic<std::size_t> m_window_count;

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
    adaptive_allocator(const adaptive_allocator<U>& other) noexcept : m_telemetry(other.m_telemetry) {}

    [[nodiscard]] T* allocate(size_type n) {
        if (n == 0) return nullptr;

        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_array_new_length();
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

        // Sized deallocation is optional: GCC enables it by default for C++14+,
        // but Clang < 19 only does with -fsized-deallocation. Use the
        // feature-test macro so the header builds on both compilers.
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
        std::size_t bytes = n * sizeof(T);
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            ::operator delete(ptr, bytes, std::align_val_t(alignof(T)));
        } else {
            ::operator delete(ptr, bytes);
        }
#else
        (void)n;
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            ::operator delete(ptr, std::align_val_t(alignof(T)));
        } else {
            ::operator delete(ptr);
        }
#endif
    }

    [[nodiscard]] std::shared_ptr<allocation_telemetry> get_telemetry() const noexcept { return m_telemetry; }

    template <typename U>
    friend bool operator==(const adaptive_allocator<T>& lhs, const adaptive_allocator<U>& rhs) noexcept {
        return lhs.m_telemetry == rhs.m_telemetry;
    }

    template <typename U>
    friend bool operator!=(const adaptive_allocator<T>& lhs, const adaptive_allocator<U>& rhs) noexcept {
        return !(lhs == rhs);
    }

  private:
    template <typename U>
    friend class adaptive_allocator;
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
    using value_type = T;
    using allocator_type = adaptive_allocator<T>;
    using size_type = std::size_t;

    adaptive_vector() : m_telemetry(std::make_shared<allocation_telemetry>()), m_alloc(m_telemetry) {}

    explicit adaptive_vector(std::shared_ptr<allocation_telemetry> telemetry)
        : m_telemetry(std::move(telemetry)), m_alloc(m_telemetry) {}

    void push_back(const T& value) {
        m_telemetry->record_insertion();
        ensure_capacity(m_data.size() + 1);
        m_data.push_back(value);
    }

    void push_back(T&& value) {
        m_telemetry->record_insertion();
        ensure_capacity(m_data.size() + 1);
        m_data.push_back(std::move(value));
    }

    template <typename... Args>
    decltype(auto) emplace_back(Args&&... args) {
        m_telemetry->record_insertion();
        ensure_capacity(m_data.size() + 1);
        return m_data.emplace_back(std::forward<Args>(args)...);
    }

    void reserve(size_type new_cap) { m_data.reserve(new_cap); }

    void shrink_to_fit() { m_data.shrink_to_fit(); }

    void clear() noexcept { m_data.clear(); }

    [[nodiscard]] size_type capacity() const noexcept { return m_data.capacity(); }
    [[nodiscard]] size_type size() const noexcept { return m_data.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_data.empty(); }
    [[nodiscard]] const std::vector<T, allocator_type>& native() const noexcept { return m_data; }

    [[nodiscard]] T& operator[](size_type idx) noexcept { return m_data[idx]; }
    [[nodiscard]] const T& operator[](size_type idx) const noexcept { return m_data[idx]; }

    [[nodiscard]] T& at(size_type idx) { return m_data.at(idx); }
    [[nodiscard]] const T& at(size_type idx) const { return m_data.at(idx); }

    [[nodiscard]] T& front() noexcept { return m_data.front(); }
    [[nodiscard]] const T& front() const noexcept { return m_data.front(); }
    [[nodiscard]] T& back() noexcept { return m_data.back(); }
    [[nodiscard]] const T& back() const noexcept { return m_data.back(); }

    [[nodiscard]] T* data() noexcept { return m_data.data(); }
    [[nodiscard]] const T* data() const noexcept { return m_data.data(); }

    using iterator = typename std::vector<T, allocator_type>::iterator;
    using const_iterator = typename std::vector<T, allocator_type>::const_iterator;

    [[nodiscard]] iterator begin() noexcept { return m_data.begin(); }
    [[nodiscard]] iterator end() noexcept { return m_data.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return m_data.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return m_data.end(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return m_data.cbegin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return m_data.cend(); }

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

}  // namespace adaptive
