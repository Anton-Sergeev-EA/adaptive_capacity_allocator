# Adaptive Capacity Allocator

[Русский](README.md) · **English** · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` is a C++17 container with the `std::vector` interface that **decides by itself how much to
grow**, based on the measured insertion rate. Under a dense stream of insertions it saves memory; with rare
insertions it grows as fast as a plain `std::vector`.

**[Interactive demo in the browser →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)** (8 languages)

## Why

When `std::vector` runs out of space it always grows by the same factor: 2× in GCC and Clang, 1.5× in MSVC.
That is fast, but right after doubling up to half of the allocation sits unused. Services that keep millions
of buffers in memory (message queues, logs, telemetry, order books) add those empty halves up to gigabytes.

`adaptive_vector` measures how fast elements are inserted and picks a growth factor for the workload:

| Insertion rate | Strategy | Growth |
|---|---|:---:|
| up to 100/s, the first insertion after an idle period, buffers under 1,024 elements | aggressive | ×2.0 |
| 100 to 1,000/s | moderate | ×1.5 |
| above 1,000/s | conservative | ×1.1 |

All thresholds and factors are configurable through `growth_policy`.

## Numbers

Linux x86_64, GCC 13.3, `-O3`, 1,000,000 `int` insertions, median of 5 runs (`acalloc bench`):

| | Time | Average spare memory while filling | Reallocations |
|---|:---:|:---:|:---:|
| `std::vector<int>` | ~4.4 ms | 36.4 % | 21 |
| `adaptive_vector<int>` 2.0 | ~8.8 ms | **5.0 %** | 81 |
| `adaptive_vector<int>` 1.0 | ~40 ms | 4.9 % | 129 |

**The price is real.** Growing by 10 % instead of doubling takes more reallocations and copies, so on a
continuous stream the container is about 2× slower than `std::vector`. In exchange, spare memory averages
about 5 % instead of 35–50 %. If memory matters more than throughput for your workload, that is a good
trade; if not, keep `std::vector`. The final capacity at any single point depends on where filling stopped:
at exactly 1,000,000 elements `std::vector` lands on its best case (2²⁰), so the fair comparison is the average
spare over the whole fill, not one final point.

Measure it on your own machine — the result depends on CPU, memory and compiler:

```bash
./build/acalloc bench
```

## Quick start

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Or after installing it (`cmake --install build`):

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Or simply copy [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) into your
project: it depends on the standard library only.

### Usage

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "capacity: " << s.capacity
              << ", reallocations: " << s.reallocations
              << ", strategy: " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### Custom growth policy

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // gentler than 1.1
p.high_threshold = 50'000;                        // conservative only above 50,000 insertions/s
p.idle_reset = std::chrono::milliseconds(500);    // idle = a pause longer than 0.5 s
adaptive::adaptive_vector<Order> book(p);
```

### One telemetry for several containers

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // decisions use the combined rate, lock-free
```

## Terminal demo in 8 languages

The `acalloc` program shows how the container behaves in four scenarios: a continuous stream, bursts, a slow
trickle, and a stream with a pause. The language is detected from the system locale.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # demonstration
./build/acalloc bench           # comparison with std::vector
./build/acalloc languages       # list languages
./build/acalloc --lang hi       # Hindi interface
```

Supported languages: Русский (primary), English, 中文, हिन्दी, Español, Français, Deutsch, Italiano.
The language can also be set with the `ACALLOC_LANG` environment variable. To add a language, see
[docs/TRANSLATING.md](docs/TRANSLATING.md).

## API

| Type | Purpose |
|---|---|
| `adaptive_vector<T>` | Container with the `std::vector` interface: `push_back`, `emplace_back`, `insert`, `emplace`, `erase`, `resize`, `assign`, `reserve`, iterators, comparisons, `swap`, plus `stats()` and `telemetry()` |
| `growth_policy` | Growth factors, rate thresholds, measurement window, idle threshold, minimum capacity for adaptation |
| `allocation_telemetry` | Thread-safe insertion-rate tracker; can be shared between containers and threads |
| `adaptive_allocator<T>` | Standard allocator that supports over-aligned types (`alignas(64)` and so on) |

Under the hood:

- **No background threads.** Version 1.0 started a thread per vector; 10,000 vectors meant 10,000 threads.
- **The clock is not read on every insertion.** The container counts insertions locally and reports them to
  the telemetry once per 64 insertions and at every growth decision. The `push_back` hot path is one counter
  and two comparisons.
- **Idle periods are detected.** If nothing was inserted for longer than `idle_reset`, old statistics are
  forgotten and the first insertion after the pause grows aggressively.

## Build, tests and checks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizers (ASan and TSan cannot be linked into one binary, so this is a three-way choice):

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

Docker builds with GCC and Clang are described in [docker/README.md](docker/README.md).
CI covers Linux (GCC and Clang, Debug and Release, C++17 and C++20), macOS, Windows (MSVC), both sanitizers,
and formatting.

## What's new

See [CHANGELOG.md](CHANGELOG.md). Code written for 1.0 (`#include "adaptive_allocator.hpp"`, the
`allocation_telemetry(idle_ms, window_ms, threshold)` constructor, `compute_capacity`, `record_insertion`) keeps
compiling unchanged.

## License

MIT — see [LICENSE](LICENSE).

---

Anton Sergeev — avsergeev1981@gmail.com
