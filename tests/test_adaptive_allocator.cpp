// Test suite for adaptive_capacity_allocator.
// Uses its own CHECK macro so the checks run in Release builds too (assert() is compiled out by NDEBUG).
#include <adaptive/adaptive_allocator.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace adaptive;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                               \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            ++g_failures;                                                                         \
            std::cerr << "    CHECK FAILED: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        }                                                                                         \
    } while (false)

#define CHECK_THROWS(expr, exc) \
    do {                        \
        bool thrown_ = false;   \
        try {                   \
            (void)(expr);       \
        } catch (const exc&) {  \
            thrown_ = true;     \
        }                       \
        CHECK(thrown_&& #expr); \
    } while (false)

void run(const char* name, const std::function<void()>& fn) {
    const int before = g_failures;
    std::cout << "[ RUN  ] " << name << std::endl;
    try {
        fn();
    } catch (const std::exception& e) {
        ++g_failures;
        std::cerr << "    unexpected exception: " << e.what() << "\n";
    }
    std::cout << (g_failures == before ? "[  OK  ] " : "[ FAIL ] ") << name << std::endl;
}

void test_basic_usage() {
    adaptive_vector<int> vec;
    CHECK(vec.empty());
    for (int i = 0; i < 1000; ++i) vec.push_back(i);
    CHECK(vec.size() == 1000);
    CHECK(vec.capacity() >= 1000);
    for (int i = 0; i < 1000; ++i) CHECK(vec[static_cast<std::size_t>(i)] == i);
    CHECK(vec.front() == 0 && vec.back() == 999);
    CHECK_THROWS(vec.at(1000), std::out_of_range);
}

void test_small_vectors_grow_aggressively() {
    // Below min_adaptive_capacity (1024 by default) growth always doubles, like std::vector.
    adaptive_vector<int> vec;
    for (int i = 0; i < 1000; ++i) vec.push_back(i);
    CHECK(vec.capacity() == 1024);
    CHECK(vec.stats().reallocations == 8);  // 8,16,32,...,1024
}

void test_growth_strategy_switch_legacy_ctor() {
    auto telemetry = std::make_shared<allocation_telemetry>(/*idle_ms=*/1000, /*window_ms=*/50, /*high=*/500.0f);
    adaptive_vector<int> vec(telemetry);
    for (int i = 0; i < 50000; ++i) vec.push_back(i);
    CHECK(vec.size() == 50000);
    CHECK(vec.stats().overhead() < 1.15);
    CHECK(vec.stats().last_strategy == growth_strategy::conservative);
}

void test_slow_trickle_is_moderate() {
    growth_policy p;
    p.min_adaptive_capacity = 0;
    p.moderate_threshold = 50.0;
    p.high_threshold = 1e7;
    adaptive_vector<int> vec(p);
    for (int i = 0; i < 40; ++i) {
        vec.push_back(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(vec.stats().last_strategy == growth_strategy::moderate);
}

void test_idle_resets_to_aggressive() {
    growth_policy p;
    p.min_adaptive_capacity = 0;
    p.idle_reset = std::chrono::milliseconds(20);
    adaptive_vector<int> vec(p);
    for (int i = 0; i < 100000; ++i) vec.push_back(i);
    CHECK(vec.stats().last_strategy == growth_strategy::conservative);

    vec.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    const auto cap = vec.capacity();
    vec.resize(cap);  // fill up without growing
    vec.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    vec.push_back(1);  // triggers one growth decision after an idle period
    CHECK(vec.stats().last_strategy == growth_strategy::aggressive);
    CHECK(vec.capacity() >= cap * 2);
}

void test_policy_math() {
    growth_policy p;
    CHECK(p.grow(1000, 1001, growth_strategy::aggressive) == 2000);
    CHECK(p.grow(1000, 1001, growth_strategy::moderate) == 1500);
    CHECK(p.grow(1000, 1001, growth_strategy::conservative) == 1100);
    CHECK(p.grow(0, 1, growth_strategy::aggressive) == 8);         // min_capacity
    CHECK(p.grow(10, 5000, growth_strategy::aggressive) == 5000);  // required wins
    CHECK(p.grow(5, 6, growth_strategy::conservative) == 8);
    const std::size_t big = std::numeric_limits<std::size_t>::max() - 1;
    CHECK(p.grow(big, big, growth_strategy::aggressive) == std::numeric_limits<std::size_t>::max());

    CHECK(p.classify(0.0) == growth_strategy::aggressive);
    CHECK(p.classify(100.0) == growth_strategy::aggressive);
    CHECK(p.classify(500.0) == growth_strategy::moderate);
    CHECK(p.classify(1e6) == growth_strategy::conservative);

    CHECK(std::string(to_string(growth_strategy::moderate)) == "moderate");
}

void test_policy_validation() {
    growth_policy bad;
    bad.conservative_factor = 1.0;
    CHECK_THROWS(bad.validate(), std::invalid_argument);
    growth_policy bad2;
    bad2.high_threshold = 10.0;  // < moderate_threshold
    CHECK_THROWS(allocation_telemetry{bad2}, std::invalid_argument);
}

void test_move_semantics() {
    adaptive_vector<int> v1;
    for (int i = 0; i < 200; ++i) v1.push_back(i);
    adaptive_vector<int> v2;
    v2 = std::move(v1);
    CHECK(v2.size() == 200);
    CHECK(v1.size() == 0);  // NOLINT(bugprone-use-after-move)
    // A moved-from vector must stay fully usable (v1 crashed here).
    for (int i = 0; i < 5000; ++i) v1.push_back(i);  // NOLINT(bugprone-use-after-move)
    CHECK(v1.size() == 5000);

    adaptive_vector<int> v3(std::move(v2));
    CHECK(v3.size() == 200 && v3[199] == 199);
}

void test_copy_and_compare() {
    adaptive_vector<int> a(std::make_shared<allocation_telemetry>());
    a = {1, 2, 3};
    adaptive_vector<int> b = a;  // a copy shares the telemetry of its source
    CHECK(a == b);
    b.push_back(4);
    CHECK(a != b);
    CHECK(a < b);
    CHECK(a.telemetry() == b.telemetry());
    adaptive_vector<int> c;
    c = b;
    CHECK(c == b);
    swap(a, c);
    CHECK(a.size() == 4 && c.size() == 3);
}

void test_full_api() {
    adaptive_vector<int> v{1, 2, 3, 4, 5};
    auto it = v.insert(v.begin() + 2, 42);
    CHECK(*it == 42 && v.size() == 6 && v[2] == 42);
    v.insert(v.end(), {7, 8});
    CHECK(v.back() == 8);
    v.insert(v.begin(), 3, -1);
    CHECK(v[0] == -1 && v[2] == -1 && v[3] == 1);
    std::vector<int> src{100, 200};
    v.insert(v.begin() + 1, src.begin(), src.end());
    CHECK(v[1] == 100 && v[2] == 200);
    v.erase(v.begin() + 1, v.begin() + 3);
    CHECK(v[1] == -1);
    v.erase(v.begin());
    v.pop_back();
    CHECK(v.back() == 7);
    v.emplace(v.begin(), 9);
    CHECK(v.front() == 9);
    int& ref = v.emplace_back(11);
    CHECK(ref == 11);
    v.resize(3);
    CHECK(v.size() == 3);
    v.resize(6, 5);
    CHECK(v.size() == 6 && v[5] == 5);
    v.assign(4, 2);
    CHECK(v.size() == 4 && v[3] == 2);
    v.assign({9, 8, 7});
    CHECK(*v.rbegin() == 7 && *v.crbegin() == 7);
    v = {1};
    CHECK(v.size() == 1);
    v.clear();
    CHECK(v.empty());
    adaptive_vector<int> n(10, 3);
    CHECK(n.size() == 10 && n[9] == 3);
    adaptive_vector<int> r(src.begin(), src.end());
    CHECK(r.size() == 2);
    CHECK(n.max_size() > 0);
    n.reserve(1000);
    CHECK(n.capacity() >= 1000);
    n.shrink_to_fit();
    CHECK(n.capacity() >= n.size());
}

void test_non_trivial_types() {
    adaptive_vector<std::string> s;
    for (int i = 0; i < 3000; ++i) s.emplace_back(std::string(40, static_cast<char>('a' + i % 26)));
    CHECK(s.size() == 3000 && s[27][0] == 'b');

    adaptive_vector<std::unique_ptr<int>> u;
    for (int i = 0; i < 3000; ++i) u.push_back(std::make_unique<int>(i));
    CHECK(*u[2999] == 2999);
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)  // structure was padded due to alignment specifier: that is the point here
#endif
struct alignas(64) wide {
    double lanes[8];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

void test_over_aligned_type() {
    adaptive_vector<wide> v;
    for (int i = 0; i < 2000; ++i) v.push_back(wide{{static_cast<double>(i)}});
    CHECK(reinterpret_cast<std::uintptr_t>(v.data()) % 64 == 0);
    CHECK(v[1999].lanes[0] == 1999.0);
}

void test_allocator_equality() {
    adaptive_allocator<int> a(std::make_shared<allocation_telemetry>());
    adaptive_allocator<long> b;
    CHECK(a == b);  // is_always_equal must agree with operator==
    CHECK(!(a != b));
    adaptive_allocator<long> c(a);
    CHECK(c.get_telemetry() == a.get_telemetry());
}

void test_multithreaded_shared_telemetry() {
    auto shared = std::make_shared<allocation_telemetry>();
    constexpr int kThreads = 8;
    constexpr int kOps = 20000;
    std::vector<std::future<std::size_t>> futures;
    for (int t = 0; t < kThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [shared] {
            adaptive_vector<int> local(shared);
            for (int i = 0; i < kOps; ++i) local.push_back(i);
            local.flush();
            return local.size();
        }));
    }
    std::size_t total = 0;
    for (auto& f : futures) total += f.get();
    CHECK(total == static_cast<std::size_t>(kThreads) * kOps);
    CHECK(shared->snapshot().insertions == static_cast<std::uint64_t>(kThreads) * kOps);
    CHECK(shared->snapshot().decisions > 0);
}

void test_many_vectors_are_cheap() {
    // v1 started a thread per vector; 10k vectors would have meant 10k threads.
    std::vector<adaptive_vector<int>> many(10000);
    for (auto& v : many) {
        for (int i = 0; i < 20; ++i) v.push_back(i);
    }
    CHECK(many.back().size() == 20);
}

}  // namespace

int main() {
    std::cout << "adaptive_capacity_allocator " << ADAPTIVE_ALLOCATOR_VERSION << " — test suite\n";
    run("basic usage", test_basic_usage);
    run("small vectors grow aggressively", test_small_vectors_grow_aggressively);
    run("high rate switches to conservative (v1 constructor)", test_growth_strategy_switch_legacy_ctor);
    run("slow trickle is moderate", test_slow_trickle_is_moderate);
    run("idle period resets to aggressive", test_idle_resets_to_aggressive);
    run("growth policy math", test_policy_math);
    run("growth policy validation", test_policy_validation);
    run("move semantics", test_move_semantics);
    run("copy and comparison", test_copy_and_compare);
    run("full std::vector-like API", test_full_api);
    run("non-trivial and move-only types", test_non_trivial_types);
    run("over-aligned types", test_over_aligned_type);
    run("allocator equality", test_allocator_equality);
    run("shared telemetry across threads", test_multithreaded_shared_telemetry);
    run("10k vectors without threads", test_many_vectors_are_cheap);

    if (g_failures != 0) {
        std::cerr << "\nFAILED: " << g_failures << " check(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "\nAll tests passed.\n";
    return EXIT_SUCCESS;
}
