# 自适应容量分配器

[Русский](README.md) · [English](README.en.md) · **中文** · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` 是一个具有 `std::vector` 接口的 C++17 容器，它会**根据实测的插入速率自行决定增长幅度**。
插入密集时节省内存；插入稀少时像普通的 `std::vector` 一样快速增长。

**[浏览器中的交互式演示 →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)**（8 种语言）

## 为什么需要它

`std::vector` 在空间不足时总是按固定倍数增长：GCC 和 Clang 为 2 倍，MSVC 为 1.5 倍。这样很快，但刚翻倍之后，
最多有一半的已分配内存处于闲置状态。对于在内存中保存数百万个缓冲区的服务（消息队列、日志、遥测、订单簿），
这些空闲的一半会累积成数 GB。

`adaptive_vector` 测量元素插入的速度，并为当前负载选择增长因子：

| 插入速率 | 策略 | 增长 |
|---|---|:---:|
| 每秒不超过 100 次、空闲后的第一次插入、小于 1,024 个元素的缓冲区 | 激进 | ×2.0 |
| 每秒 100 到 1,000 次 | 适中 | ×1.5 |
| 每秒超过 1,000 次 | 保守 | ×1.1 |

所有阈值和增长因子都可以通过 `growth_policy` 配置。

## 数据

Linux x86_64，GCC 13.3，`-O3`，插入 1,000,000 个 `int`，5 次运行取中位数（`acalloc bench`）：

| | 耗时 | 填充过程中的平均冗余内存 | 重新分配次数 |
|---|:---:|:---:|:---:|
| `std::vector<int>` | 约 4.4 毫秒 | 36.4 % | 21 |
| `adaptive_vector<int>` 2.0 | 约 8.8 毫秒 | **5.0 %** | 81 |
| `adaptive_vector<int>` 1.0 | 约 40 毫秒 | 4.9 % | 129 |

**代价是真实存在的。** 每次增长 10% 而不是翻倍，需要更多的重新分配和复制，因此在连续插入时容器比 `std::vector`
慢约 2 倍。作为交换，平均冗余内存约为 5%，而不是 35–50%。如果对你的负载来说内存比吞吐量更重要，这很划算；
否则请继续使用 `std::vector`。某一时刻的最终容量取决于填充停在哪里：恰好 1,000,000 个元素时 `std::vector`
正好处于最佳情况（2²⁰），因此公平的比较是整个填充过程中的平均冗余，而不是最后一个点。

请在自己的机器上测量——结果取决于处理器、内存和编译器：

```bash
./build/acalloc bench
```

## 快速开始

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

或者在安装之后（`cmake --install build`）：

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

也可以直接把 [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) 复制到你的项目中：
它只依赖标准库。

### 用法

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "容量: " << s.capacity
              << ", 重新分配: " << s.reallocations
              << ", 策略: " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### 自定义增长策略

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // 比 1.1 更温和
p.high_threshold = 50'000;                        // 每秒超过 50,000 次插入才使用保守策略
p.idle_reset = std::chrono::milliseconds(500);    // 停顿超过 0.5 秒视为空闲
adaptive::adaptive_vector<Order> book(p);
```

### 多个容器共享一个遥测对象

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // 按总速率决策，无锁
```

## 8 种语言的终端演示

`acalloc` 程序在四个场景中展示容器的行为：连续插入、突发、缓慢插入以及带停顿的插入。语言根据系统区域设置自动检测。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # 演示
./build/acalloc bench           # 与 std::vector 比较
./build/acalloc languages       # 语言列表
./build/acalloc --lang zh       # 中文界面
```

支持的语言：Русский（主要语言）、English、中文、हिन्दी、Español、Français、Deutsch、Italiano。
也可以通过环境变量 `ACALLOC_LANG` 设置语言。添加新语言的方法见 [docs/TRANSLATING.md](docs/TRANSLATING.md)。

## API

| 类型 | 用途 |
|---|---|
| `adaptive_vector<T>` | 具有 `std::vector` 接口的容器：`push_back`、`emplace_back`、`insert`、`emplace`、`erase`、`resize`、`assign`、`reserve`、迭代器、比较、`swap`，以及 `stats()` 和 `telemetry()` |
| `growth_policy` | 增长因子、速率阈值、测量窗口、空闲阈值、开始自适应的最小容量 |
| `allocation_telemetry` | 线程安全的插入速率统计；可在容器和线程之间共享 |
| `adaptive_allocator<T>` | 支持超对齐类型（如 `alignas(64)`）的标准分配器 |

内部实现：

- **没有后台线程。** 1.0 版本为每个向量启动一个线程；10,000 个向量就意味着 10,000 个线程。
- **不会在每次插入时读取时钟。** 容器在本地计数，每 64 次插入以及每次增长决策时才向遥测报告。`push_back`
  的热路径只有一个计数器和两次比较。
- **能够识别空闲。** 如果超过 `idle_reset` 没有插入，旧统计会被丢弃，停顿后的第一次插入会激进增长。

## 构建、测试与检查

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizer（ASan 和 TSan 不能链接到同一个程序中，因此需三选一）：

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

使用 GCC 和 Clang 的 Docker 构建见 [docker/README.md](docker/README.md)。
CI 覆盖 Linux（GCC 和 Clang、Debug 和 Release、C++17 和 C++20）、macOS、Windows（MSVC）、两种 sanitizer 以及代码格式。

## 更新内容

见 [CHANGELOG.md](CHANGELOG.md)。为 1.0 编写的代码（`#include "adaptive_allocator.hpp"`、构造函数
`allocation_telemetry(idle_ms, window_ms, threshold)`、`compute_capacity`、`record_insertion`）无需修改即可继续编译。

## 许可证

MIT — 见 [LICENSE](LICENSE)。

---

Anton Sergeev — avsergeev1981@gmail.com
