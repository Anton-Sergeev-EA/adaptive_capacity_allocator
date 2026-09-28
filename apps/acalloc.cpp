// acalloc — multilingual demo and benchmark for adaptive_capacity_allocator.
// SPDX-License-Identifier: MIT
#include <adaptive/adaptive_allocator.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "i18n.hpp"

#ifndef ACALLOC_DEFAULT_COMMAND
#define ACALLOC_DEFAULT_COMMAND "demo"
#endif

namespace {

using namespace acalloc::i18n;
using clock_type = std::chrono::steady_clock;

struct result {
    double ms = 0.0;
    double avg_spare = 0.0;  // mean (capacity - size) / size over the whole fill, in %
    std::size_t size = 0;
    std::size_t capacity = 0;
    std::uint64_t reallocations = 0;
    std::string strategy;  // stable id, empty for std::vector
    double rate = 0.0;
};

double elapsed_ms(clock_type::time_point start) {
    return std::chrono::duration<double, std::milli>(clock_type::now() - start).count();
}

// Capacity change points (size right after the push, new capacity). Recording only the change points
// keeps the per-push cost to one comparison, identical for both containers.
struct growth_trace {
    std::vector<std::pair<std::size_t, std::size_t>> points;

    template <typename Vec>
    auto wrap(Vec& v) {
        return [this, &v](int x) {
            const auto cap = v.capacity();
            v.push_back(x);
            if (v.capacity() != cap) points.emplace_back(v.size(), v.capacity());
        };
    }

    // Sum over every intermediate size s of (cap(s) - s), divided by the sum of s.
    [[nodiscard]] double average_spare_percent(std::size_t final_size) const {
        long double spare = 0, used = 0;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto from = static_cast<long double>(points[i].first);
            const auto to = static_cast<long double>(i + 1 < points.size() ? points[i + 1].first - 1 : final_size);
            if (to < from) continue;
            const long double n = to - from + 1;
            const long double sum_s = (from + to) * n / 2;
            spare += static_cast<long double>(points[i].second) * n - sum_s;
            used += sum_s;
        }
        return used > 0 ? static_cast<double>(100 * spare / used) : 0.0;
    }
};

template <typename Body>
result run_std(Body&& body) {
    std::vector<int> v;
    growth_trace trace;
    const auto start = clock_type::now();
    body(trace.wrap(v));
    result r;
    r.ms = elapsed_ms(start);
    r.size = v.size();
    r.capacity = v.capacity();
    r.reallocations = trace.points.size();
    r.avg_spare = trace.average_spare_percent(v.size());
    return r;
}

template <typename Body>
result run_adaptive(adaptive::adaptive_vector<int>& v, Body&& body) {
    growth_trace trace;
    const auto start = clock_type::now();
    body(trace.wrap(v));
    result r;
    r.ms = elapsed_ms(start);
    const auto s = v.stats();
    r.size = s.size;
    r.capacity = s.capacity;
    r.reallocations = s.reallocations;
    r.strategy = adaptive::to_string(s.last_strategy);
    r.rate = s.last_rate;
    r.avg_spare = trace.average_spare_percent(s.size);
    return r;
}

std::string kv(msg key, const std::string& value) {
    return std::string(tr(key)) + std::string(tr(msg::kv_sep)) + value;
}

void print_row(std::string_view name, const result& r) {
    std::string label(name);
    label.resize(std::max<std::size_t>(label.size(), 17), ' ');
    const double spare =
        r.size == 0 ? 0.0 : 100.0 * static_cast<double>(r.capacity - r.size) / static_cast<double>(r.size);
    std::cout << "  " << label << kv(msg::k_time, format_fixed(r.ms, 1) + " " + std::string(tr(msg::unit_ms))) << " · "
              << kv(msg::k_capacity, format_int(r.capacity)) << " · " << kv(msg::k_spare, format_fixed(spare, 1) + " %")
              << " · " << kv(msg::k_reallocs, format_int(r.reallocations));
    if (!r.strategy.empty()) std::cout << " · " << kv(msg::k_strategy, std::string(strategy_name(r.strategy)));
    std::cout << "\n";
}

void print_verdict(const result& std_r, const result& ad_r) {
    std::cout << tr(msg::v_avg_spare, {format_fixed(ad_r.avg_spare, 1) + " %", format_fixed(std_r.avg_spare, 1) + " %"})
              << "\n";
    const auto diff_elems = static_cast<std::int64_t>(std_r.capacity) - static_cast<std::int64_t>(ad_r.capacity);
    const double diff_bytes = static_cast<double>(diff_elems) * sizeof(int);
    if (diff_elems >= 0) {
        std::cout << tr(msg::v_saved, {format_bytes(diff_bytes), format_int(static_cast<std::uint64_t>(diff_elems))})
                  << "\n";
    } else {
        std::cout << tr(msg::v_more_memory,
                        {format_bytes(-diff_bytes), format_int(static_cast<std::uint64_t>(-diff_elems))})
                  << "\n";
    }
    const double ratio = std_r.ms > 0.0 ? ad_r.ms / std_r.ms : 1.0;
    if (ratio > 1.15) {
        std::cout << tr(msg::v_slower, {format_fixed(ratio, 1)}) << "\n";
    } else if (ratio < 0.87) {
        std::cout << tr(msg::v_faster, {format_fixed(1.0 / ratio, 1)}) << "\n";
    } else {
        std::cout << tr(msg::v_same_speed) << "\n";
    }
    if (ad_r.reallocations > std_r.reallocations) {
        std::cout << tr(msg::v_reallocs, {format_int(ad_r.reallocations), format_int(std_r.reallocations)}) << "\n";
    }
}

void heading(const std::string& text) {
    std::cout << "\n■ " << text << "\n";
}

void banner() {
    std::cout << tr(msg::title) << " " << ADAPTIVE_ALLOCATOR_VERSION << "\n" << tr(msg::tagline) << "\n";
}

void scenario_steady(int elements) {
    heading(tr(msg::s_steady, {format_int(static_cast<std::uint64_t>(elements))}));
    auto body = [elements](auto&& push) {
        for (int i = 0; i < elements; ++i) push(i);
    };
    const result s = run_std(body);
    adaptive::adaptive_vector<int> v;
    const result a = run_adaptive(v, body);
    print_row("std::vector", s);
    print_row("adaptive_vector", a);
    print_verdict(s, a);
}

void scenario_burst() {
    static constexpr int kBatches = 40;
    static constexpr int kBatch = 5000;
    heading(tr(msg::s_burst, {format_int(kBatches), format_int(kBatch)}));
    auto body = [](auto&& push) {
        for (int b = 0; b < kBatches; ++b) {
            for (int i = 0; i < kBatch; ++i) push(b * kBatch + i);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };
    const result s = run_std(body);
    adaptive::adaptive_vector<int> v;
    const result a = run_adaptive(v, body);
    print_row("std::vector", s);
    print_row("adaptive_vector", a);
    print_verdict(s, a);
}

adaptive::growth_policy demo_policy() {
    adaptive::growth_policy p;
    p.min_adaptive_capacity = 64;
    p.idle_reset = std::chrono::milliseconds(200);
    return p;
}

void scenario_trickle() {
    static constexpr int kCount = 300;
    static constexpr int kEveryMs = 2;
    heading(tr(msg::s_trickle, {format_int(kCount), format_int(kEveryMs)}));
    std::cout << "  " << tr(msg::demo_policy_note, {format_int(64), format_int(200)}) << "\n";
    adaptive::adaptive_vector<int> v(demo_policy());
    const result a = run_adaptive(v, [](auto&& push) {
        for (int i = 0; i < kCount; ++i) {
            push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(kEveryMs));
        }
    });
    print_row("adaptive_vector", a);
    std::cout << tr(msg::v_trickle,
                    {format_int(static_cast<std::uint64_t>(a.rate)), std::string(strategy_name(a.strategy))})
              << "\n";
}

void scenario_idle() {
    static constexpr int kPauseMs = 400;
    heading(tr(msg::s_idle, {format_int(kPauseMs)}));
    std::cout << "  " << tr(msg::demo_policy_note, {format_int(64), format_int(200)}) << "\n";
    adaptive::adaptive_vector<int> v(demo_policy());
    int i = 0;
    for (; i < 200000; ++i) v.push_back(i);
    while (v.size() < v.capacity()) v.push_back(i++);  // fill the buffer completely
    const auto before = v.stats();
    std::this_thread::sleep_for(std::chrono::milliseconds(kPauseMs));
    v.push_back(i);  // the first insertion after the pause triggers a growth decision
    const auto after = v.stats();
    std::cout << tr(msg::v_idle,
                    {std::string(strategy_name(adaptive::to_string(before.last_strategy))) + " (" +
                         format_int(before.capacity) + ")",
                     std::string(strategy_name(adaptive::to_string(after.last_strategy))) + " (" +
                         format_int(after.capacity) + ")"})
              << "\n";
}

int command_demo(int elements) {
    banner();
    scenario_steady(elements);
    scenario_burst();
    scenario_trickle();
    scenario_idle();
    std::cout << "\n" << tr(msg::summary) << "\n";
    return EXIT_SUCCESS;
}

int command_bench(int elements) {
    static constexpr int kRuns = 5;
    banner();
    heading(tr(msg::bench_header, {format_int(kRuns), format_int(static_cast<std::uint64_t>(elements))}));
    auto body = [elements](auto&& push) {
        for (int i = 0; i < elements; ++i) push(i);
    };
    run_std(body);  // warm-up: page faults, CPU frequency
    std::vector<result> std_runs, ad_runs;
    for (int r = 0; r < kRuns; ++r) {
        std_runs.push_back(run_std(body));
        adaptive::adaptive_vector<int> v;
        ad_runs.push_back(run_adaptive(v, body));
    }
    auto median = [](std::vector<result> runs) {
        std::sort(runs.begin(), runs.end(), [](const result& a, const result& b) { return a.ms < b.ms; });
        return runs[runs.size() / 2];
    };
    const result s = median(std_runs);
    const result a = median(ad_runs);
    print_row("std::vector", s);
    print_row("adaptive_vector", a);
    print_verdict(s, a);
    std::cout << "\n" << tr(msg::bench_note) << "\n";
    return EXIT_SUCCESS;
}

int command_languages() {
    std::cout << tr(msg::languages_header) << "\n";
    for (const auto& l : kLanguages) {
        std::cout << (l.code == current_language() ? "  * " : "    ") << l.code << "  " << l.native_name;
        if (l.native_name != l.english_name) std::cout << " (" << l.english_name << ")";
        std::cout << "\n";
    }
    std::cout << tr(msg::languages_hint) << "\n";
    return EXIT_SUCCESS;
}

std::string language_codes() {
    std::string out;
    for (const auto& l : kLanguages) {
        if (!out.empty()) out += ", ";
        out += l.code;
    }
    return out;
}

int fail(const std::string& message, std::string_view prog) {
    std::cerr << message << "\n" << tr(msg::hint_help, {prog}) << "\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    prepare_console();
    set_language(detect_language());

    const std::string_view prog = "acalloc";
    std::string command = ACALLOC_DEFAULT_COMMAND;
    int elements = 1'000'000;
    bool show_help = false;
    bool show_version = false;

    // First pass: language, so that every later message is already translated.
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        std::string_view value;
        if (a.rfind("--lang=", 0) == 0) {
            value = a.substr(7);
        } else if (a == "--lang" && i + 1 < argc) {
            value = argv[i + 1];
        } else {
            continue;
        }
        if (!set_language(value)) {
            return fail(tr(msg::err_bad_language, {value, language_codes()}), prog);
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--help" || a == "-h") {
            show_help = true;
        } else if (a == "--version" || a == "-V") {
            show_version = true;
        } else if (a == "--lang") {
            ++i;  // value already handled
        } else if (a.rfind("--lang=", 0) == 0) {
            continue;
        } else if (a == "--elements" || a.rfind("--elements=", 0) == 0) {
            std::string value;
            if (a == "--elements") {
                if (i + 1 >= argc) return fail(tr(msg::err_bad_number, {"--elements"}), prog);
                value = argv[++i];
            } else {
                value = std::string(a.substr(11));
            }
            char* end = nullptr;
            const long n = std::strtol(value.c_str(), &end, 10);
            if (value.empty() || *end != '\0' || n <= 0 || n > 200'000'000) {
                return fail(tr(msg::err_bad_number, {"--elements"}), prog);
            }
            elements = static_cast<int>(n);
        } else if (a == "demo" || a == "bench" || a == "languages") {
            command = std::string(a);
        } else {
            return fail(tr(msg::err_unknown_option, {a}), prog);
        }
    }

    if (show_help) {
        std::cout << tr(msg::title) << " — " << tr(msg::tagline) << "\n\n" << tr(msg::help, {prog}) << "\n";
        return EXIT_SUCCESS;
    }
    if (show_version) {
        std::cout << tr(msg::version_line, {tr(msg::title), ADAPTIVE_ALLOCATOR_VERSION}) << "\n";
        return EXIT_SUCCESS;
    }
    if (command == "languages") return command_languages();
    if (command == "bench") return command_bench(elements);
    return command_demo(elements);
}
