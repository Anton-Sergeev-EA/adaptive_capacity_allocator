// Adaptive Capacity Allocator — telemetry-driven growth policy for C++ containers.
// SPDX-License-Identifier: MIT
// Copyright (c) Anton Sergeev
//
// Header-only. Requires C++17. No dependencies beyond the standard library.
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#define ADAPTIVE_ALLOCATOR_VERSION_MAJOR 2
#define ADAPTIVE_ALLOCATOR_VERSION_MINOR 0
#define ADAPTIVE_ALLOCATOR_VERSION_PATCH 0
#define ADAPTIVE_ALLOCATOR_VERSION "2.0.0"

namespace adaptive {

/// How aggressively a container grows when it runs out of capacity.
enum class growth_strategy : std::uint8_t {
    aggressive,   ///< Default 2.0x: rare or idle insertions, minimise reallocations.
    moderate,     ///< Default 1.5x: balanced.
    conservative  ///< Default 1.1x: sustained high-rate insertions, minimise memory overhead.
};

/// Stable, language-neutral identifier of a strategy ("aggressive", "moderate", "conservative").
[[nodiscard]] constexpr const char* to_string(growth_strategy s) noexcept {
    switch (s) {
        case growth_strategy::aggressive:
            return "aggressive";
        case growth_strategy::moderate:
            return "moderate";
        case growth_strategy::conservative:
            return "conservative";
    }
    return "unknown";
}

/**
 * @brief Tunable parameters of the adaptive growth policy.
 *
 * The insertion rate is measured in elements per second over the interval between two
 * consecutive growth decisions (capped by @ref history_window). Below @ref moderate_threshold
 * the container grows aggressively, above @ref high_threshold conservatively, and moderately
 * in between.
 */
struct growth_policy {
    double aggressive_factor = 2.0;
    double moderate_factor = 1.5;
    double conservative_factor = 1.1;

    double moderate_threshold = 100.0;  ///< elements/s at which `moderate` starts.
    double high_threshold = 1000.0;     ///< elements/s at which `conservative` starts.

    /// Longest interval the rate is averaged over. Older activity is forgotten.
    std::chrono::milliseconds history_window{100};

    /// If nothing was inserted for this long, the rate is treated as zero (→ aggressive).
    std::chrono::milliseconds idle_reset{1000};

    /// Below this capacity the container always grows aggressively: small buffers cost little
    /// memory, and doubling them avoids a storm of tiny reallocations.
    std::size_t min_adaptive_capacity = 1024;

    /// Smallest capacity ever allocated by a growth step.
    std::size_t min_capacity = 8;

    /// adaptive_vector reports its insertions (one clock read + one atomic add) once per this many
    /// insertions. Smaller = sharper idle detection, larger = cheaper hot path.
    std::uint32_t report_batch = 64;

    /// Throws std::invalid_argument if the policy is inconsistent.
    void validate() const {
        if (!(aggressive_factor > 1.0) || !(moderate_factor > 1.0) || !(conservative_factor > 1.0)) {
            throw std::invalid_argument("adaptive::growth_policy: growth factors must be > 1.0");
        }
        if (!(moderate_threshold >= 0.0) || !(high_threshold >= moderate_threshold)) {
            throw std::invalid_argument("adaptive::growth_policy: need 0 <= moderate_threshold <= high_threshold");
        }
        if (report_batch == 0) {
            throw std::invalid_argument("adaptive::growth_policy: report_batch must be >= 1");
        }
        if (history_window.count() <= 0 || idle_reset.count() <= 0) {
            throw std::invalid_argument("adaptive::growth_policy: time windows must be positive");
        }
    }

    [[nodiscard]] double factor(growth_strategy s) const noexcept {
        switch (s) {
            case growth_strategy::aggressive:
                return aggressive_factor;
            case growth_strategy::moderate:
                return moderate_factor;
            case growth_strategy::conservative:
                return conservative_factor;
        }
        return aggressive_factor;
    }

    [[nodiscard]] growth_strategy classify(double rate_per_second) const noexcept {
        if (rate_per_second > high_threshold) return growth_strategy::conservative;
        if (rate_per_second > moderate_threshold) return growth_strategy::moderate;
        return growth_strategy::aggressive;
    }

    /// Pure growth step: next capacity for @p current when at least @p required is needed.
    [[nodiscard]] std::size_t grow(std::size_t current, std::size_t required, growth_strategy s) const noexcept {
        constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
        const double scaled = static_cast<double>(current) * factor(s);
        std::size_t next = scaled >= static_cast<double>(kMax) ? kMax : static_cast<std::size_t>(scaled);
        if (next <= current && current < kMax) next = current + 1;  // always make progress
        return std::max({next, required, min_capacity});
    }
};

/// Result of one growth decision.
struct growth_decision {
    std::size_t capacity = 0;  ///< New capacity to allocate.
    growth_strategy strategy = growth_strategy::aggressive;
    double rate = 0.0;  ///< Insertion rate (elements/s) the decision was based on.
};

/// Snapshot of what a telemetry object has observed.
struct telemetry_snapshot {
    std::uint64_t insertions = 0;  ///< Total insertions reported.
    std::uint64_t decisions = 0;   ///< Growth decisions taken (≈ reallocations).
    double last_rate = 0.0;        ///< Rate (elements/s) seen at the last decision.
    growth_strategy last_strategy = growth_strategy::aggressive;
};

/**
 * @brief Thread-safe insertion-rate tracker shared by one or more containers.
 *
 * Design for low overhead:
 *  - no background thread (v1 started one thread per container);
 *  - the clock is never read per insertion: containers batch their counts and report them
 *    once per growth_policy::report_batch insertions and at every growth decision;
 *  - the hot path of push_back() is a non-atomic counter and two comparisons.
 *
 * All members are lock-free atomics; any number of threads may share one instance.
 */
class allocation_telemetry {
  public:
    using clock = std::chrono::steady_clock;

    allocation_telemetry() : allocation_telemetry(growth_policy{}) {}

    explicit allocation_telemetry(const growth_policy& policy)
        : m_policy(policy), m_window_start(now_ns()), m_last_activity(now_ns()) {
        m_policy.validate();
    }

    /// v1-compatible constructor: idle period, history window (ms) and high-rate threshold (elements/s).
    allocation_telemetry(std::size_t idle_ms, std::size_t history_window_ms, float high_threshold)
        : allocation_telemetry(make_legacy_policy(idle_ms, history_window_ms, high_threshold)) {}

    allocation_telemetry(const allocation_telemetry&) = delete;
    allocation_telemetry& operator=(const allocation_telemetry&) = delete;

    [[nodiscard]] const growth_policy& policy() const noexcept { return m_policy; }

    /// Reports one insertion (v1 API). Containers batch their counts instead; see decide().
    void record_insertion() noexcept { record_insertions(1); }

    /// Reports @p n insertions made just now (one atomic add and one clock read).
    /// adaptive_vector calls this once per growth_policy::report_batch insertions.
    void record_insertions(std::uint64_t n) noexcept {
        if (n == 0) return;
        m_total.fetch_add(n, std::memory_order_relaxed);
        m_last_activity.store(now_ns(), std::memory_order_relaxed);
    }

    /// Insertion rate (elements/s) averaged over the current measurement window. Does not change state.
    [[nodiscard]] double current_rate() const noexcept {
        return window_rate(now_ns(), m_total.load(std::memory_order_relaxed));
    }

    /// Strategy a decision would pick right now from the window rate alone. Does not change state.
    [[nodiscard]] growth_strategy calculate_strategy() const noexcept { return m_policy.classify(current_rate()); }

    /**
     * @brief Takes one growth decision.
     *
     * @param current_cap     capacity of the buffer that is full.
     * @param required_cap    capacity needed right now.
     * @param new_insertions  insertions the caller made since its previous report.
     *
     * Normally the rate is the window rate: every insertion reported by all containers sharing
     * this telemetry, averaged since the window start (the window rolls after
     * growth_policy::history_window). If nothing was reported for longer than
     * growth_policy::idle_reset, the container was idle: the old window is forgotten and the rate is
     * just @p new_insertions over the idle gap, so the first insertions after a pause grow aggressively.
     *
     * Capacities below growth_policy::min_adaptive_capacity always grow aggressively.
     */
    [[nodiscard]] growth_decision decide(std::size_t current_cap,
                                         std::size_t required_cap,
                                         std::uint64_t new_insertions = 0) noexcept {
        const std::int64_t now = now_ns();
        const std::int64_t last = m_last_activity.exchange(now, std::memory_order_relaxed);
        const std::uint64_t total = m_total.fetch_add(new_insertions, std::memory_order_relaxed) + new_insertions;
        const std::int64_t idle_gap = std::max<std::int64_t>(now - last, kMinElapsedNs);

        double rate = 0.0;
        if (idle_gap > to_ns(m_policy.idle_reset)) {
            rate = static_cast<double>(new_insertions) * 1e9 / static_cast<double>(idle_gap);
            m_window_start.store(now, std::memory_order_relaxed);
            m_window_base.store(total, std::memory_order_relaxed);
        } else {
            rate = window_rate(now, total);
            if (now - m_window_start.load(std::memory_order_relaxed) >= to_ns(m_policy.history_window)) {
                m_window_start.store(now, std::memory_order_relaxed);
                m_window_base.store(total, std::memory_order_relaxed);
            }
        }

        const growth_strategy s =
            current_cap >= m_policy.min_adaptive_capacity ? m_policy.classify(rate) : growth_strategy::aggressive;

        m_decisions.fetch_add(1, std::memory_order_relaxed);
        m_last_rate.store(rate, std::memory_order_relaxed);
        m_last_strategy.store(static_cast<std::uint8_t>(s), std::memory_order_relaxed);
        return growth_decision{m_policy.grow(current_cap, required_cap, s), s, rate};
    }

    /// v1 API: decide() without batched insertions; returns only the capacity.
    [[nodiscard]] std::size_t compute_capacity(std::size_t current_cap, std::size_t required_cap) noexcept {
        return decide(current_cap, required_cap).capacity;
    }

    [[nodiscard]] telemetry_snapshot snapshot() const noexcept {
        telemetry_snapshot out;
        out.insertions = m_total.load(std::memory_order_relaxed);
        out.decisions = m_decisions.load(std::memory_order_relaxed);
        out.last_rate = m_last_rate.load(std::memory_order_relaxed);
        out.last_strategy = static_cast<growth_strategy>(m_last_strategy.load(std::memory_order_relaxed));
        return out;
    }

  private:
    static constexpr std::int64_t kMinElapsedNs = 1000;  // 1 µs floor: never divide by ~0.

    static growth_policy make_legacy_policy(std::size_t idle_ms, std::size_t window_ms, float high) {
        growth_policy p;
        p.idle_reset = std::chrono::milliseconds(static_cast<std::int64_t>(idle_ms));
        p.history_window = std::chrono::milliseconds(static_cast<std::int64_t>(window_ms));
        p.high_threshold = static_cast<double>(high);
        p.moderate_threshold = std::min(p.moderate_threshold, p.high_threshold);
        return p;
    }

    [[nodiscard]] static std::int64_t now_ns() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count();
    }

    [[nodiscard]] static std::int64_t to_ns(std::chrono::milliseconds ms) noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(ms).count();
    }

    [[nodiscard]] double window_rate(std::int64_t now, std::uint64_t total) const noexcept {
        const std::uint64_t base = m_window_base.load(std::memory_order_relaxed);
        const std::uint64_t count = total >= base ? total - base : 0;
        const std::int64_t elapsed =
            std::max<std::int64_t>(now - m_window_start.load(std::memory_order_relaxed), kMinElapsedNs);
        return static_cast<double>(count) * 1e9 / static_cast<double>(elapsed);
    }

    growth_policy m_policy;

    std::atomic<std::uint64_t> m_total{0};
    std::atomic<std::uint64_t> m_window_base{0};
    std::atomic<std::int64_t> m_window_start;
    std::atomic<std::int64_t> m_last_activity;

    std::atomic<std::uint64_t> m_decisions{0};
    std::atomic<double> m_last_rate{0.0};
    std::atomic<std::uint8_t> m_last_strategy{0};
};

/**
 * @brief Standard-conforming allocator that handles over-aligned types.
 *
 * Allocation does not depend on any state, so all instances compare equal
 * (is_always_equal). The optional telemetry pointer is carried along purely so that
 * containers built on this allocator can reach it.
 */
template <typename T>
class adaptive_allocator {
  public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    template <typename U>
    struct rebind {
        using other = adaptive_allocator<U>;
    };

    adaptive_allocator() noexcept = default;

    explicit adaptive_allocator(std::shared_ptr<allocation_telemetry> telemetry) noexcept
        : m_telemetry(std::move(telemetry)) {}

    template <typename U>
    adaptive_allocator(const adaptive_allocator<U>& other) noexcept  // NOLINT(google-explicit-constructor)
        : m_telemetry(other.m_telemetry) {}

    [[nodiscard]] T* allocate(size_type n) {
        if (n == 0) return nullptr;
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        const std::size_t bytes = n * sizeof(T);
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            return static_cast<T*>(::operator new(bytes, std::align_val_t(alignof(T))));
        } else {
            return static_cast<T*>(::operator new(bytes));
        }
    }

    void deallocate(T* ptr, size_type n) noexcept {
        if (!ptr) return;
            // Sized deallocation is optional (Clang < 19 needs -fsized-deallocation), so use the
            // feature-test macro and fall back to unsized delete.
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
        const std::size_t bytes = n * sizeof(T);
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
    friend bool operator==(const adaptive_allocator&, const adaptive_allocator<U>&) noexcept {
        return true;
    }
    template <typename U>
    friend bool operator!=(const adaptive_allocator&, const adaptive_allocator<U>&) noexcept {
        return false;
    }

  private:
    template <typename U>
    friend class adaptive_allocator;
    std::shared_ptr<allocation_telemetry> m_telemetry;
};

/// Per-container statistics.
struct vector_stats {
    std::size_t size = 0;
    std::size_t capacity = 0;
    std::uint64_t reallocations = 0;  ///< Growth steps taken by this container.
    growth_strategy last_strategy = growth_strategy::aggressive;
    double last_rate = 0.0;  ///< Insertion rate (elements/s) seen at the last growth step.

    [[nodiscard]] double overhead() const noexcept {
        return size == 0 ? 0.0 : static_cast<double>(capacity) / static_cast<double>(size);
    }
};

/**
 * @brief std::vector-like container whose growth factor adapts to the measured insertion rate.
 *
 * The hot path of push_back()/emplace_back() is a size check plus a non-atomic counter; the
 * telemetry is consulted only when the buffer must grow. Several containers may share one
 * allocation_telemetry (e.g. per subsystem), in which case decisions use their combined rate.
 */
template <typename T>
class adaptive_vector {
    using storage = std::vector<T, adaptive_allocator<T>>;

  public:
    using value_type = T;
    using allocator_type = adaptive_allocator<T>;
    using size_type = typename storage::size_type;
    using difference_type = typename storage::difference_type;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = typename storage::iterator;
    using const_iterator = typename storage::const_iterator;
    using reverse_iterator = typename storage::reverse_iterator;
    using const_reverse_iterator = typename storage::const_reverse_iterator;

    // --- construction -------------------------------------------------------------------

    adaptive_vector() = default;

    explicit adaptive_vector(std::shared_ptr<allocation_telemetry> telemetry)
        : m_telemetry(std::move(telemetry)), m_data(allocator_type(m_telemetry)) {}

    explicit adaptive_vector(const growth_policy& policy)
        : adaptive_vector(std::make_shared<allocation_telemetry>(policy)) {}

    explicit adaptive_vector(size_type count) : m_data(count) {}
    adaptive_vector(size_type count, const T& value) : m_data(count, value) {}
    adaptive_vector(std::initializer_list<T> init) : m_data(init) {}

    template <typename InputIt, typename = std::enable_if_t<!std::is_integral<InputIt>::value>>
    adaptive_vector(InputIt first, InputIt last) : m_data(first, last) {}

    adaptive_vector(const adaptive_vector& other)
        : m_telemetry(other.m_telemetry), m_data(other.m_data), m_reallocations(0) {}

    adaptive_vector(adaptive_vector&& other) noexcept
        : m_telemetry(std::move(other.m_telemetry)),
          m_data(std::move(other.m_data)),
          m_pending(std::exchange(other.m_pending, 0)),
          m_reallocations(std::exchange(other.m_reallocations, 0)),
          m_last(other.m_last) {}

    adaptive_vector& operator=(const adaptive_vector& other) {
        if (this != &other) {
            m_data = other.m_data;
            if (!m_telemetry) m_telemetry = other.m_telemetry;
        }
        return *this;
    }

    adaptive_vector& operator=(adaptive_vector&& other) noexcept {
        if (this != &other) {
            m_telemetry = std::move(other.m_telemetry);
            m_data = std::move(other.m_data);
            m_pending = std::exchange(other.m_pending, 0);
            m_reallocations = std::exchange(other.m_reallocations, 0);
            m_last = other.m_last;
        }
        return *this;
    }

    adaptive_vector& operator=(std::initializer_list<T> init) {
        assign(init);
        return *this;
    }

    ~adaptive_vector() { flush(); }

    // --- element access -----------------------------------------------------------------

    [[nodiscard]] reference operator[](size_type i) noexcept { return m_data[i]; }
    [[nodiscard]] const_reference operator[](size_type i) const noexcept { return m_data[i]; }
    [[nodiscard]] reference at(size_type i) { return m_data.at(i); }
    [[nodiscard]] const_reference at(size_type i) const { return m_data.at(i); }
    [[nodiscard]] reference front() noexcept { return m_data.front(); }
    [[nodiscard]] const_reference front() const noexcept { return m_data.front(); }
    [[nodiscard]] reference back() noexcept { return m_data.back(); }
    [[nodiscard]] const_reference back() const noexcept { return m_data.back(); }
    [[nodiscard]] T* data() noexcept { return m_data.data(); }
    [[nodiscard]] const T* data() const noexcept { return m_data.data(); }

    // --- iterators ----------------------------------------------------------------------

    [[nodiscard]] iterator begin() noexcept { return m_data.begin(); }
    [[nodiscard]] iterator end() noexcept { return m_data.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return m_data.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return m_data.end(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return m_data.cbegin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return m_data.cend(); }
    [[nodiscard]] reverse_iterator rbegin() noexcept { return m_data.rbegin(); }
    [[nodiscard]] reverse_iterator rend() noexcept { return m_data.rend(); }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return m_data.rbegin(); }
    [[nodiscard]] const_reverse_iterator rend() const noexcept { return m_data.rend(); }
    [[nodiscard]] const_reverse_iterator crbegin() const noexcept { return m_data.crbegin(); }
    [[nodiscard]] const_reverse_iterator crend() const noexcept { return m_data.crend(); }

    // --- capacity -----------------------------------------------------------------------

    [[nodiscard]] bool empty() const noexcept { return m_data.empty(); }
    [[nodiscard]] size_type size() const noexcept { return m_data.size(); }
    [[nodiscard]] size_type max_size() const noexcept { return m_data.max_size(); }
    [[nodiscard]] size_type capacity() const noexcept { return m_data.capacity(); }
    void reserve(size_type new_cap) { m_data.reserve(new_cap); }
    void shrink_to_fit() { m_data.shrink_to_fit(); }

    // --- modifiers ----------------------------------------------------------------------

    void clear() noexcept { m_data.clear(); }

    void push_back(const T& value) {
        grow_for(1);
        m_data.push_back(value);
    }

    void push_back(T&& value) {
        grow_for(1);
        m_data.push_back(std::move(value));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        grow_for(1);
        return m_data.emplace_back(std::forward<Args>(args)...);
    }

    iterator insert(const_iterator pos, const T& value) { return emplace(pos, value); }
    iterator insert(const_iterator pos, T&& value) { return emplace(pos, std::move(value)); }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        const auto idx = pos - cbegin();
        grow_for(count);
        return m_data.insert(m_data.cbegin() + idx, count, value);
    }

    template <typename InputIt, typename = std::enable_if_t<!std::is_integral<InputIt>::value>>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        const auto idx = pos - cbegin();
        if constexpr (std::is_base_of<std::forward_iterator_tag,
                                      typename std::iterator_traits<InputIt>::iterator_category>::value) {
            grow_for(static_cast<size_type>(std::distance(first, last)));
        }
        return m_data.insert(m_data.cbegin() + idx, first, last);
    }

    iterator insert(const_iterator pos, std::initializer_list<T> init) { return insert(pos, init.begin(), init.end()); }

    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        const auto idx = pos - cbegin();
        grow_for(1);
        return m_data.emplace(m_data.cbegin() + idx, std::forward<Args>(args)...);
    }

    iterator erase(const_iterator pos) { return m_data.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_data.erase(first, last); }
    void pop_back() { m_data.pop_back(); }

    void resize(size_type count) {
        if (count > size()) grow_for(count - size());
        m_data.resize(count);
    }

    void resize(size_type count, const T& value) {
        if (count > size()) grow_for(count - size());
        m_data.resize(count, value);
    }

    void assign(size_type count, const T& value) { m_data.assign(count, value); }
    template <typename InputIt, typename = std::enable_if_t<!std::is_integral<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        m_data.assign(first, last);
    }
    void assign(std::initializer_list<T> init) { m_data.assign(init); }

    void swap(adaptive_vector& other) noexcept {
        using std::swap;
        swap(m_telemetry, other.m_telemetry);
        m_data.swap(other.m_data);
        swap(m_pending, other.m_pending);
        swap(m_reallocations, other.m_reallocations);
        swap(m_last, other.m_last);
    }

    // --- adaptive extras ----------------------------------------------------------------

    /// Telemetry used by this container (created lazily on the first growth step).
    [[nodiscard]] std::shared_ptr<allocation_telemetry> telemetry() const noexcept { return m_telemetry; }

    [[nodiscard]] vector_stats stats() const noexcept {
        vector_stats s;
        s.size = size();
        s.capacity = capacity();
        s.reallocations = m_reallocations;
        s.last_strategy = m_last.strategy;
        s.last_rate = m_last.rate;
        return s;
    }

    /// Reports locally batched insertions to the telemetry now (otherwise done at the next growth step).
    void flush() noexcept {
        if (m_pending != 0 && m_telemetry) {
            m_telemetry->record_insertions(m_pending);
        }
        m_pending = 0;
    }

    [[nodiscard]] const storage& native() const noexcept { return m_data; }
    [[nodiscard]] allocator_type get_allocator() const noexcept { return m_data.get_allocator(); }

    friend bool operator==(const adaptive_vector& a, const adaptive_vector& b) { return a.m_data == b.m_data; }
    friend bool operator!=(const adaptive_vector& a, const adaptive_vector& b) { return a.m_data != b.m_data; }
    friend bool operator<(const adaptive_vector& a, const adaptive_vector& b) { return a.m_data < b.m_data; }
    friend bool operator<=(const adaptive_vector& a, const adaptive_vector& b) { return a.m_data <= b.m_data; }
    friend bool operator>(const adaptive_vector& a, const adaptive_vector& b) { return a.m_data > b.m_data; }
    friend bool operator>=(const adaptive_vector& a, const adaptive_vector& b) { return a.m_data >= b.m_data; }
    friend void swap(adaptive_vector& a, adaptive_vector& b) noexcept { a.swap(b); }

  private:
    // Hot path: count locally, take a decision only when the buffer is actually full.
    void grow_for(size_type extra) {
        m_pending += extra;
        const size_type required = m_data.size() + extra;
        if (required > m_data.capacity()) {
            grow_slow(required);
        } else if (m_pending >= m_report_batch && m_telemetry) {
            flush();
        }
    }

    void grow_slow(size_type required) {
        if (!m_telemetry) m_telemetry = std::make_shared<allocation_telemetry>();
        m_report_batch = m_telemetry->policy().report_batch;
        m_last = m_telemetry->decide(m_data.capacity(), required, std::exchange(m_pending, 0));
        m_data.reserve(std::max(std::min(m_last.capacity, max_size()), required));
        ++m_reallocations;
    }

    std::shared_ptr<allocation_telemetry> m_telemetry;
    storage m_data;
    std::uint64_t m_pending = 0;
    std::uint64_t m_reallocations = 0;
    std::uint64_t m_report_batch = 64;
    growth_decision m_last{};
};

}  // namespace adaptive
