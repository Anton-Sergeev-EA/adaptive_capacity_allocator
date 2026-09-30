# Allocatore di capacità adattivo

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · **Italiano**

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` è un contenitore C++17 con l'interfaccia di `std::vector` che **decide da solo quanto crescere**
in base alla velocità di inserimento misurata. Con un flusso denso di inserimenti risparmia memoria; con
inserimenti rari cresce veloce quanto un normale `std::vector`.

**[Demo interattiva nel browser →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)** (8 lingue)

## A cosa serve

Quando `std::vector` esaurisce lo spazio, cresce sempre dello stesso fattore: 2× con GCC e Clang, 1,5× con MSVC.
È veloce, ma subito dopo il raddoppio fino a metà della memoria riservata resta inutilizzata. Nei servizi che
tengono in memoria milioni di buffer (code di messaggi, log, telemetria, book di ordini), quelle metà vuote
arrivano a gigabyte.

`adaptive_vector` misura a che velocità vengono inseriti gli elementi e sceglie il fattore di crescita in base al carico:

| Velocità di inserimento | Strategia | Crescita |
|---|---|:---:|
| fino a 100/s, il primo inserimento dopo l'inattività, buffer sotto i 1.024 elementi | aggressiva | ×2,0 |
| da 100 a 1.000/s | moderata | ×1,5 |
| oltre 1.000/s | conservativa | ×1,1 |

Tutte le soglie e i fattori si configurano tramite `growth_policy`.

## Numeri

Linux x86_64, GCC 13.3, `-O3`, 1.000.000 di inserimenti di `int`, mediana di 5 esecuzioni (`acalloc bench`):

| | Tempo | Margine medio di memoria durante il riempimento | Riallocazioni |
|---|:---:|:---:|:---:|
| `std::vector<int>` | ~4,4 ms | 36,4 % | 21 |
| `adaptive_vector<int>` 2.0 | ~8,8 ms | **5,0 %** | 81 |
| `adaptive_vector<int>` 1.0 | ~40 ms | 4,9 % | 129 |

**Il prezzo è reale.** Crescere del 10 % invece di raddoppiare richiede più riallocazioni e copie, quindi su un
flusso continuo il contenitore è circa 2 volte più lento di `std::vector`. In cambio il margine medio di memoria è
intorno al 5 % invece del 35–50 %. Se per il tuo carico la memoria conta più della velocità, conviene; altrimenti
resta con `std::vector`. La capacità finale in un singolo punto dipende da dove si ferma il riempimento: con
esattamente 1.000.000 di elementi `std::vector` capita nel suo caso migliore (2²⁰), quindi il confronto onesto è il
margine medio su tutto il riempimento, non un singolo punto finale.

Misuralo sulla tua macchina: il risultato dipende da processore, memoria e compilatore.

```bash
./build/acalloc bench
```

## Per iniziare

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Oppure dopo l'installazione (`cmake --install build`):

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Oppure copia semplicemente [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) nel tuo
progetto: dipende solo dalla libreria standard.

### Uso

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "capacità: " << s.capacity
              << ", riallocazioni: " << s.reallocations
              << ", strategia: " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### Politica di crescita personalizzata

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // più morbido di 1.1
p.high_threshold = 50'000;                        // conservativa solo oltre 50.000 inserimenti/s
p.idle_reset = std::chrono::milliseconds(500);    // inattività = pausa oltre 0,5 s
adaptive::adaptive_vector<Order> book(p);
```

### Una telemetria per più contenitori

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // decisioni sulla velocità complessiva, senza lock
```

## Demo nel terminale in 8 lingue

Il programma `acalloc` mostra il comportamento del contenitore in quattro scenari: flusso continuo, raffiche,
gocciolamento lento e flusso con pausa. La lingua viene rilevata dalle impostazioni locali del sistema.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # dimostrazione
./build/acalloc bench           # confronto con std::vector
./build/acalloc languages       # elenco delle lingue
./build/acalloc --lang it       # interfaccia in italiano
```

Lingue supportate: Русский (principale), English, 中文, हिन्दी, Español, Français, Deutsch, Italiano.
La lingua si può impostare anche con la variabile d'ambiente `ACALLOC_LANG`. Per aggiungere una lingua, vedi
[docs/TRANSLATING.md](docs/TRANSLATING.md).

## API

| Tipo | Scopo |
|---|---|
| `adaptive_vector<T>` | Contenitore con l'interfaccia di `std::vector`: `push_back`, `emplace_back`, `insert`, `emplace`, `erase`, `resize`, `assign`, `reserve`, iteratori, confronti, `swap`, più `stats()` e `telemetry()` |
| `growth_policy` | Fattori di crescita, soglie di velocità, finestra di misura, soglia di inattività, capacità minima per l'adattamento |
| `allocation_telemetry` | Contatore della velocità di inserimento thread-safe; condivisibile tra contenitori e thread |
| `adaptive_allocator<T>` | Allocatore standard con supporto per tipi sovra-allineati (`alignas(64)` ecc.) |

Sotto il cofano:

- **Nessun thread in background.** La versione 1.0 avviava un thread per vettore; 10.000 vettori significavano
  10.000 thread.
- **L'orologio non viene letto a ogni inserimento.** Il contenitore conta gli inserimenti localmente e li comunica
  alla telemetria ogni 64 inserimenti e a ogni decisione di crescita. Il percorso veloce di `push_back` è un
  contatore e due confronti.
- **L'inattività viene rilevata.** Se non ci sono inserimenti per più di `idle_reset`, le vecchie statistiche
  vengono dimenticate e il primo inserimento dopo la pausa cresce in modo aggressivo.

## Compilazione, test e verifiche

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizer (ASan e TSan non si possono collegare nello stesso eseguibile, quindi si sceglie una di tre opzioni):

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

Le build Docker con GCC e Clang sono descritte in [docker/README.md](docker/README.md).
La CI copre Linux (GCC e Clang, Debug e Release, C++17 e C++20), macOS, Windows (MSVC), entrambi i sanitizer
e la formattazione.

## Novità

Vedi [CHANGELOG.md](CHANGELOG.md). Il codice scritto per la 1.0 (`#include "adaptive_allocator.hpp"`, il costruttore
`allocation_telemetry(idle_ms, window_ms, threshold)`, `compute_capacity`, `record_insertion`) continua a compilare
senza modifiche.

## Licenza

MIT — vedi [LICENSE](LICENSE).

---

Anton Sergeev — avsergeev1981@gmail.com
