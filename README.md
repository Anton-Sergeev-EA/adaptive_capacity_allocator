# Adaptive Capacity Allocator

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A C++17/20 telemetry-driven allocator and `std::vector` wrapper that picks a
capacity growth factor at runtime based on measured insertion rate, instead of
using a single fixed factor (2.0x in GCC/Clang, 1.5x in MSVC) for every
workload.

## Architecture & Motivation

Standard containers use one hardcoded growth factor regardless of how they are
used. `adaptive_vector` decouples that decision from the container: a shared
`allocation_telemetry` object tracks how frequently elements are being
inserted and picks a growth strategy per reallocation:

| Insertion rate       | Strategy               | Growth factor |
|-----------------------|-------------------------|:---:|
| Idle (>1s since last insert) or low rate (<100/s) | Aggressive  | 2.0x |
| Medium rate (100-1000/s, configurable)            | Moderate    | 1.5x |
| High rate (above the configured threshold)        | Conservative| 1.1x |

## Features

- **Runtime-adaptive growth** between Conservative (1.1x), Moderate (1.5x),
  and Aggressive (2.0x), chosen from a rolling-window insertion-rate estimate.
- **Lock-free telemetry** — atomic counters, no locks on the insertion hot
  path.
- **Background idle monitoring** — a worker thread resets stale rate data
  after a configurable idle period, using `std::condition_variable` rather
  than a sleep-polling loop.
- **Over-aligned type support** — safely allocates types requiring alignment
  greater than `__STDCPP_DEFAULT_NEW_ALIGNMENT__` (e.g. AVX-512/SIMD types)
  via C++17 `std::align_val_t`.
- **Header-only, zero dependencies** beyond the standard library and threads.

## Honest Benchmark Numbers

*Measured on this machine (Linux x86_64, GCC 13, `-O3`), 1,000,000
insertions. Run `./adaptive_benchmark` yourself — hardware and compiler
matter a lot for numbers like these.*

| Container | Execution Time | Memory Overhead (Capacity/Size) | Final Capacity |
| :--- | :---: | :---: | :---: |
| `std::vector<int>` | ~6 ms | 1.049 | 1,048,576 |
| `adaptive::adaptive_vector<int>` | ~41 ms | **1.017** | 1,017,019 |

**The trade-off is real, not free.** Under continuous high-rate insertion the
conservative (1.1x) strategy engages almost immediately and stays engaged,
which is exactly what it's designed to do — but growing by 10% instead of
doubling means roughly 7x more reallocations (and therefore ~7x more element
copies) to reach the same final size. That shows up directly as ~7x slower
wall-clock time in this benchmark, in exchange for ~3% less capacity overhead.
Whether that trade is worth it depends entirely on whether your workload is
memory-constrained or throughput-constrained — this library does not make
that call for you, it just makes the growth factor adjustable at runtime
based on measured behavior instead of fixed at compile time.

For workloads that are genuinely bursty (occasional insertions separated by
idle periods) rather than sustained-maximum-throughput, the aggressive
fallback keeps reallocation counts low during the burst itself while the
telemetry engine watches for sustained high-frequency periods worth trading
speed for memory on.

## Quick Start

### Basic Usage

```cpp
#include "adaptive_allocator.hpp"
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> vec;

    for (int i = 0; i < 100000; ++i) {
        vec.push_back(i);
    }

    std::cout << "Size: " << vec.size() << "\n";
    std::cout << "Capacity: " << vec.capacity() << "\n";
    std::cout << "Overhead Factor: " << static_cast<double>(vec.capacity()) / vec.size() << "\n";

    return 0;
}
```

### Integration via CMake

```cmake
include(FetchContent)

FetchContent_Declare(
    adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_target PRIVATE adaptive::allocator)
```

### Building Tests and Benchmarks Manually

```bash
git clone https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
cd adaptive_capacity_allocator

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

ctest --output-on-failure
./adaptive_benchmark
```

## Debugging with Sanitizers

ASan and TSan instrument incompatible things and cannot be linked into the
same binary, so this is a three-way choice (`none` / `address` / `thread`),
not a single on/off switch:

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc) && ./adaptive_tests

# ThreadSanitizer (this library's whole point is concurrent telemetry, so
# this is the more important one to run when touching allocation_telemetry)
cmake -DADAPTIVE_SANITIZER=thread -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc) && ./adaptive_tests
```

## License

Distributed under the MIT License — see [LICENSE](LICENSE) for details.

---

Anton Sergeev — avsergeev1981@gmail.com
