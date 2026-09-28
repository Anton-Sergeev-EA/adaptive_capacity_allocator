// Localisation for the acalloc command-line tool.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace acalloc::i18n {

struct language {
    std::string_view code;         ///< ISO 639-1 code.
    std::string_view native_name;  ///< Name in the language itself.
    std::string_view english_name;
};

/// Supported languages: Russian first (primary), then by number of speakers worldwide.
inline constexpr std::array<language, 8> kLanguages{{
    {"ru", "Русский", "Russian"},
    {"en", "English", "English"},
    {"zh", "中文", "Chinese"},
    {"hi", "हिन्दी", "Hindi"},
    {"es", "Español", "Spanish"},
    {"fr", "Français", "French"},
    {"de", "Deutsch", "German"},
    {"it", "Italiano", "Italian"},
}};

inline constexpr std::string_view kDefaultLanguage = "ru";

enum class msg : std::uint16_t {
    title,
    tagline,
    help,
    version_line,
    err_unknown_option,
    err_bad_language,
    err_bad_number,
    hint_help,
    languages_header,
    languages_hint,
    kv_sep,
    s_steady,
    s_burst,
    s_trickle,
    s_idle,
    demo_policy_note,
    k_time,
    k_capacity,
    k_spare,
    k_reallocs,
    k_strategy,
    k_rate,
    strat_aggressive,
    strat_moderate,
    strat_conservative,
    v_avg_spare,
    v_saved,
    v_more_memory,
    v_slower,
    v_faster,
    v_same_speed,
    v_reallocs,
    v_trickle,
    v_idle,
    summary,
    bench_header,
    bench_note,
    unit_ms,
    unit_rate,
    unit_b,
    unit_kb,
    unit_mb,
    count_  // must stay last
};

/// True if @p code is one of kLanguages.
[[nodiscard]] bool is_supported(std::string_view code) noexcept;

/// Selects the interface language; returns false (and keeps the current one) if unsupported.
bool set_language(std::string_view code) noexcept;

[[nodiscard]] std::string_view current_language() noexcept;

/// Language from the environment: ACALLOC_LANG, LC_ALL, LC_MESSAGES, LANG, LANGUAGE, then the OS
/// user locale on Windows. Falls back to Russian.
[[nodiscard]] std::string detect_language();

/// Translated text for the current language.
[[nodiscard]] std::string_view tr(msg id) noexcept;

/// Translated text with {0}, {1}, ... replaced by @p args.
[[nodiscard]] std::string tr(msg id, std::initializer_list<std::string_view> args);

/// Integer with locale-appropriate digit grouping (Indian lakh/crore grouping for Hindi).
[[nodiscard]] std::string format_int(std::uint64_t value);

/// Fixed-point number with the locale's decimal separator.
[[nodiscard]] std::string format_fixed(double value, int decimals);

/// Byte count as B / KB / MB in the current language.
[[nodiscard]] std::string format_bytes(double bytes);

/// Localised name of a growth strategy given its stable id ("aggressive", "moderate", "conservative").
[[nodiscard]] std::string_view strategy_name(std::string_view id) noexcept;

/// Makes the Windows console print UTF-8 correctly; no-op elsewhere.
void prepare_console() noexcept;

}  // namespace acalloc::i18n
