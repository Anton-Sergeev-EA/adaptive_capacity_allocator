# Адаптивный распределитель ёмкости

**Русский** · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` — контейнер C++17, похожий на `std::vector`, который **сам выбирает, насколько расти**,
по реальной скорости вставки элементов. При плотном потоке вставок он бережёт память, при редких — растёт
так же быстро, как обычный `std::vector`.

**[Интерактивная демонстрация в браузере →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)** (8 языков)

## Зачем это нужно

`std::vector` при нехватке места всегда увеличивает буфер в одно и то же число раз: в 2 раза в GCC и Clang,
в 1,5 раза в MSVC. Это быстро, но после удвоения до половины выделенной памяти может простаивать.
Для сервисов, которые держат в памяти миллионы буферов (очереди сообщений, журналы, телеметрия,
биржевые стаканы), эти пустые половины складываются в гигабайты.

`adaptive_vector` измеряет, как быстро в него вставляют элементы, и подбирает коэффициент роста под нагрузку:

| Скорость вставки | Стратегия | Рост |
|---|---|:---:|
| до 100 в секунду, первая вставка после простоя, буфер меньше 1 024 элементов | агрессивная | ×2,0 |
| от 100 до 1 000 в секунду | умеренная | ×1,5 |
| больше 1 000 в секунду | консервативная | ×1,1 |

Все пороги и коэффициенты настраиваются через `growth_policy`.

## Цифры

Linux x86_64, GCC 13.3, `-O3`, 1 000 000 вставок `int`, медиана из 5 запусков (`acalloc bench`):

| | Время | Средний запас памяти за время заполнения | Перераспределений |
|---|:---:|:---:|:---:|
| `std::vector<int>` | ~4,4 мс | 36,4 % | 21 |
| `adaptive_vector<int>` 2.0 | ~8,8 мс | **5,0 %** | 81 |
| `adaptive_vector<int>` 1.0 | ~40 мс | 4,9 % | 129 |

**Цена реальна.** Рост на 10 % вместо удвоения требует больше перераспределений и копирований, поэтому на
непрерывном потоке контейнер примерно в 2 раза медленнее `std::vector`. Зато лишней памяти в среднем
около 5 % вместо 35–50 %. Если для вашей задачи память важнее пропускной способности, обмен выгоден;
если нет, оставьте `std::vector`. Итоговая ёмкость в конкретной точке зависит от того, где остановилось
заполнение: ровно на 1 000 000 элементов `std::vector` попадает в свой лучший случай (2²⁰), поэтому честное
сравнение — средний запас за всё время, а не одна последняя точка.

Запустите замер у себя — результат зависит от процессора, памяти и компилятора:

```bash
./build/acalloc bench
```

## Быстрый старт

### Подключение через CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Или после установки (`cmake --install build`):

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Или просто скопируйте файл [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) в свой проект:
он зависит только от стандартной библиотеки.

### Использование

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "ёмкость: " << s.capacity
              << ", перераспределений: " << s.reallocations
              << ", стратегия: " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### Своя политика роста

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // мягче, чем 1.1
p.high_threshold = 50'000;                        // консервативно только выше 50 000 вставок/с
p.idle_reset = std::chrono::milliseconds(500);    // простой — пауза дольше 0,5 с
adaptive::adaptive_vector<Order> book(p);
```

### Одна телеметрия на несколько контейнеров

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // решения по суммарной скорости, без блокировок
```

## Демонстрация в терминале на 8 языках

Программа `acalloc` показывает поведение контейнера на четырёх сценариях: непрерывный поток, всплески,
медленный ручеёк и поток с паузой. Язык определяется автоматически по языку системы.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # демонстрация
./build/acalloc bench           # сравнение с std::vector
./build/acalloc languages       # список языков
./build/acalloc --lang hi       # интерфейс на хинди
```

Поддерживаемые языки: русский (основной), English, 中文, हिन्दी, Español, Français, Deutsch, Italiano.
Язык также задаётся переменной окружения `ACALLOC_LANG`. Как добавить язык — в [docs/TRANSLATING.md](docs/TRANSLATING.md).

## API

| Тип | Назначение |
|---|---|
| `adaptive_vector<T>` | Контейнер с интерфейсом `std::vector`: `push_back`, `emplace_back`, `insert`, `emplace`, `erase`, `resize`, `assign`, `reserve`, итераторы, сравнения, `swap`, а также `stats()` и `telemetry()` |
| `growth_policy` | Коэффициенты роста, пороги скорости, окно измерения, порог простоя, минимальная ёмкость для адаптации |
| `allocation_telemetry` | Потокобезопасный счётчик скорости вставки; можно разделять между контейнерами и потоками |
| `adaptive_allocator<T>` | Стандартный аллокатор с поддержкой типов с повышенным выравниванием (`alignas(64)` и т. п.) |

Устройство изнутри:

- **Нет фоновых потоков.** Версия 1.0 запускала поток на каждый вектор; 10 000 векторов означали 10 000 потоков.
- **Часы не опрашиваются на каждой вставке.** Контейнер считает вставки локально и сообщает телеметрии раз
  в 64 вставки и при каждом решении о росте. Горячий путь `push_back` — один счётчик и два сравнения.
- **Простой распознаётся.** Если вставок не было дольше `idle_reset`, старая статистика забывается, и первая
  же вставка после паузы приводит к агрессивному росту.

## Сборка, тесты и проверки

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Санитайзеры (ASan и TSan нельзя собрать в один файл, поэтому это выбор из трёх вариантов):

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

Сборка в Docker с GCC и Clang описана в [docker/README.md](docker/README.md).
CI проверяет Linux (GCC и Clang, Debug и Release, C++17 и C++20), macOS, Windows (MSVC),
оба санитайзера и форматирование.

## Что нового

Полный список изменений — в [CHANGELOG.md](CHANGELOG.md). Код версии 1.0 (`#include "adaptive_allocator.hpp"`,
конструктор `allocation_telemetry(idle_ms, window_ms, threshold)`, `compute_capacity`, `record_insertion`)
продолжает компилироваться без изменений.

## Лицензия

MIT — см. [LICENSE](LICENSE).

---

Антон Сергеев — avsergeev1981@gmail.com
