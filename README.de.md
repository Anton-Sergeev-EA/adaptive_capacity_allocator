# Adaptiver Kapazitätsallokator

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · **Deutsch** · [Italiano](README.it.md)

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` ist ein C++17-Container mit der Schnittstelle von `std::vector`, der **selbst entscheidet, wie
stark er wächst** – anhand der gemessenen Einfügerate. Bei dichtem Einfügestrom spart er Speicher, bei seltenen
Einfügungen wächst er so schnell wie ein gewöhnlicher `std::vector`.

**[Interaktive Demo im Browser →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)** (8 Sprachen)

## Wozu

Wenn `std::vector` der Platz ausgeht, wächst er immer um denselben Faktor: 2× bei GCC und Clang, 1,5× bei MSVC.
Das ist schnell, aber direkt nach dem Verdoppeln liegt bis zur Hälfte des reservierten Speichers brach. Bei
Diensten, die Millionen von Puffern im Speicher halten (Nachrichtenwarteschlangen, Logs, Telemetrie, Orderbücher),
summieren sich diese leeren Hälften zu Gigabytes.

`adaptive_vector` misst, wie schnell Elemente eingefügt werden, und wählt den Wachstumsfaktor passend zur Last:

| Einfügerate | Strategie | Wachstum |
|---|---|:---:|
| bis 100/s, die erste Einfügung nach Leerlauf, Puffer unter 1.024 Elementen | aggressiv | ×2,0 |
| 100 bis 1.000/s | moderat | ×1,5 |
| über 1.000/s | konservativ | ×1,1 |

Alle Schwellen und Faktoren lassen sich über `growth_policy` einstellen.

## Zahlen

Linux x86_64, GCC 13.3, `-O3`, 1.000.000 `int`-Einfügungen, Median aus 5 Läufen (`acalloc bench`):

| | Zeit | Mittlere Speicherreserve beim Füllen | Umlagerungen |
|---|:---:|:---:|:---:|
| `std::vector<int>` | ~4,4 ms | 36,4 % | 21 |
| `adaptive_vector<int>` 2.0 | ~8,8 ms | **5,0 %** | 81 |
| `adaptive_vector<int>` 1.0 | ~40 ms | 4,9 % | 129 |

**Der Preis ist echt.** Um 10 % statt aufs Doppelte zu wachsen erfordert mehr Umlagerungen und Kopien, daher ist
der Container bei kontinuierlichem Strom etwa 2-mal langsamer als `std::vector`. Dafür liegt die mittlere
Speicherreserve bei rund 5 % statt 35–50 %. Wenn für Ihre Last Speicher wichtiger ist als Durchsatz, lohnt sich
der Tausch; sonst bleiben Sie bei `std::vector`. Die Endkapazität an einem einzelnen Punkt hängt davon ab, wo das
Füllen endet: Bei genau 1.000.000 Elementen trifft `std::vector` seinen besten Fall (2²⁰). Fair ist daher der
Vergleich der mittleren Reserve über das gesamte Füllen, nicht eines einzelnen Endpunkts.

Messen Sie selbst – das Ergebnis hängt von CPU, Speicher und Compiler ab:

```bash
./build/acalloc bench
```

## Schnellstart

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Oder nach der Installation (`cmake --install build`):

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Oder kopieren Sie einfach [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) in Ihr
Projekt: Die Datei hängt nur von der Standardbibliothek ab.

### Verwendung

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "Kapazität: " << s.capacity
              << ", Umlagerungen: " << s.reallocations
              << ", Strategie: " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### Eigene Wachstumsstrategie

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // sanfter als 1.1
p.high_threshold = 50'000;                        // konservativ erst ab 50.000 Einfügungen/s
p.idle_reset = std::chrono::milliseconds(500);    // Leerlauf = Pause länger als 0,5 s
adaptive::adaptive_vector<Order> book(p);
```

### Eine Telemetrie für mehrere Container

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // Entscheidungen nach gemeinsamer Rate, ohne Sperren
```

## Terminal-Demo in 8 Sprachen

Das Programm `acalloc` zeigt das Verhalten des Containers in vier Szenarien: kontinuierlicher Strom, Schübe,
langsames Tröpfeln und Strom mit Pause. Die Sprache wird aus den Systemeinstellungen erkannt.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # Demonstration
./build/acalloc bench           # Vergleich mit std::vector
./build/acalloc languages       # Sprachen auflisten
./build/acalloc --lang de       # Oberfläche auf Deutsch
```

Unterstützte Sprachen: Русский (Hauptsprache), English, 中文, हिन्दी, Español, Français, Deutsch, Italiano.
Die Sprache lässt sich auch über die Umgebungsvariable `ACALLOC_LANG` festlegen. Wie man eine Sprache hinzufügt,
steht in [docs/TRANSLATING.md](docs/TRANSLATING.md).

## API

| Typ | Zweck |
|---|---|
| `adaptive_vector<T>` | Container mit der Schnittstelle von `std::vector`: `push_back`, `emplace_back`, `insert`, `emplace`, `erase`, `resize`, `assign`, `reserve`, Iteratoren, Vergleiche, `swap`, dazu `stats()` und `telemetry()` |
| `growth_policy` | Wachstumsfaktoren, Ratenschwellen, Messfenster, Leerlaufschwelle, Mindestkapazität für die Anpassung |
| `allocation_telemetry` | Threadsicherer Zähler der Einfügerate; zwischen Containern und Threads teilbar |
| `adaptive_allocator<T>` | Standard-Allokator mit Unterstützung für überausgerichtete Typen (`alignas(64)` usw.) |

Unter der Haube:

- **Keine Hintergrund-Threads.** Version 1.0 startete pro Vektor einen Thread; 10.000 Vektoren bedeuteten
  10.000 Threads.
- **Die Uhr wird nicht bei jeder Einfügung gelesen.** Der Container zählt Einfügungen lokal und meldet sie der
  Telemetrie alle 64 Einfügungen und bei jeder Wachstumsentscheidung. Der schnelle Pfad von `push_back` besteht
  aus einem Zähler und zwei Vergleichen.
- **Leerlauf wird erkannt.** Gab es länger als `idle_reset` keine Einfügungen, wird die alte Statistik vergessen,
  und die erste Einfügung nach der Pause wächst aggressiv.

## Bauen, Tests und Prüfungen

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizer (ASan und TSan lassen sich nicht in ein Programm linken, daher eine Auswahl aus drei Varianten):

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

Docker-Builds mit GCC und Clang sind in [docker/README.md](docker/README.md) beschrieben.
Die CI prüft Linux (GCC und Clang, Debug und Release, C++17 und C++20), macOS, Windows (MSVC), beide Sanitizer
und die Formatierung.

## Neuerungen

Siehe [CHANGELOG.md](CHANGELOG.md). Für 1.0 geschriebener Code (`#include "adaptive_allocator.hpp"`, der Konstruktor
`allocation_telemetry(idle_ms, window_ms, threshold)`, `compute_capacity`, `record_insertion`) kompiliert
unverändert weiter.

## Lizenz

MIT – siehe [LICENSE](LICENSE).

---

Anton Sergeev — avsergeev1981@gmail.com
