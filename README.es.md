# Asignador de capacidad adaptativo

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · **Español** · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` es un contenedor C++17 con la interfaz de `std::vector` que **decide por sí mismo cuánto
crecer** según la velocidad de inserción medida. Con un flujo denso de inserciones ahorra memoria; con
inserciones escasas crece tan rápido como un `std::vector` normal.

**[Demostración interactiva en el navegador →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)** (8 idiomas)

## Para qué sirve

Cuando a `std::vector` le falta espacio, siempre crece en el mismo factor: 2× en GCC y Clang, 1,5× en MSVC.
Es rápido, pero justo después de duplicarse hasta la mitad de la memoria reservada queda sin usar. En servicios
que mantienen millones de búferes en memoria (colas de mensajes, registros, telemetría, libros de órdenes),
esas mitades vacías suman gigabytes.

`adaptive_vector` mide a qué velocidad se insertan los elementos y elige el factor de crecimiento según la carga:

| Velocidad de inserción | Estrategia | Crecimiento |
|---|---|:---:|
| hasta 100/s, la primera inserción tras la inactividad, búferes de menos de 1.024 elementos | agresiva | ×2,0 |
| de 100 a 1.000/s | moderada | ×1,5 |
| más de 1.000/s | conservadora | ×1,1 |

Todos los umbrales y factores se configuran con `growth_policy`.

## Cifras

Linux x86_64, GCC 13.3, `-O3`, 1.000.000 de inserciones de `int`, mediana de 5 ejecuciones (`acalloc bench`):

| | Tiempo | Holgura media de memoria durante el llenado | Reasignaciones |
|---|:---:|:---:|:---:|
| `std::vector<int>` | ~4,4 ms | 36,4 % | 21 |
| `adaptive_vector<int>` 2.0 | ~8,8 ms | **5,0 %** | 81 |
| `adaptive_vector<int>` 1.0 | ~40 ms | 4,9 % | 129 |

**El precio es real.** Crecer un 10 % en lugar de duplicar exige más reasignaciones y copias, por lo que con un
flujo continuo el contenedor es unas 2 veces más lento que `std::vector`. A cambio, la holgura media de memoria
ronda el 5 % en lugar del 35–50 %. Si para su carga la memoria importa más que el rendimiento, compensa; si no,
siga con `std::vector`. La capacidad final en un punto concreto depende de dónde se detuvo el llenado: con
exactamente 1.000.000 de elementos `std::vector` cae en su mejor caso (2²⁰), así que la comparación justa es la
holgura media durante todo el llenado, no un único punto final.

Mídalo en su equipo: el resultado depende del procesador, la memoria y el compilador.

```bash
./build/acalloc bench
```

## Primeros pasos

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

O después de instalarlo (`cmake --install build`):

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

O simplemente copie [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) a su
proyecto: solo depende de la biblioteca estándar.

### Uso

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "capacidad: " << s.capacity
              << ", reasignaciones: " << s.reallocations
              << ", estrategia: " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### Política de crecimiento propia

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // más suave que 1.1
p.high_threshold = 50'000;                        // conservadora solo por encima de 50.000 inserciones/s
p.idle_reset = std::chrono::milliseconds(500);    // inactividad = pausa de más de 0,5 s
adaptive::adaptive_vector<Order> book(p);
```

### Una telemetría para varios contenedores

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // decisiones según la velocidad conjunta, sin bloqueos
```

## Demostración en la terminal en 8 idiomas

El programa `acalloc` muestra el comportamiento del contenedor en cuatro escenarios: flujo continuo, ráfagas,
goteo lento y flujo con pausa. El idioma se detecta a partir de la configuración regional del sistema.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # demostración
./build/acalloc bench           # comparación con std::vector
./build/acalloc languages       # lista de idiomas
./build/acalloc --lang es       # interfaz en español
```

Idiomas disponibles: Русский (principal), English, 中文, हिन्दी, Español, Français, Deutsch, Italiano.
El idioma también se puede fijar con la variable de entorno `ACALLOC_LANG`. Para añadir un idioma, consulte
[docs/TRANSLATING.md](docs/TRANSLATING.md).

## API

| Tipo | Función |
|---|---|
| `adaptive_vector<T>` | Contenedor con la interfaz de `std::vector`: `push_back`, `emplace_back`, `insert`, `emplace`, `erase`, `resize`, `assign`, `reserve`, iteradores, comparaciones, `swap`, además de `stats()` y `telemetry()` |
| `growth_policy` | Factores de crecimiento, umbrales de velocidad, ventana de medición, umbral de inactividad, capacidad mínima para adaptarse |
| `allocation_telemetry` | Medidor de velocidad de inserción seguro entre hilos; se puede compartir entre contenedores e hilos |
| `adaptive_allocator<T>` | Asignador estándar compatible con tipos sobrealineados (`alignas(64)`, etc.) |

Por dentro:

- **Sin hilos en segundo plano.** La versión 1.0 lanzaba un hilo por vector; 10.000 vectores eran 10.000 hilos.
- **El reloj no se consulta en cada inserción.** El contenedor cuenta las inserciones localmente y las comunica a
  la telemetría cada 64 inserciones y en cada decisión de crecimiento. El camino rápido de `push_back` es un
  contador y dos comparaciones.
- **La inactividad se detecta.** Si no hubo inserciones durante más de `idle_reset`, se olvidan las estadísticas
  antiguas y la primera inserción tras la pausa crece de forma agresiva.

## Compilación, pruebas y comprobaciones

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizers (ASan y TSan no se pueden enlazar en un mismo binario, así que es una elección entre tres opciones):

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

La compilación en Docker con GCC y Clang se describe en [docker/README.md](docker/README.md).
La CI cubre Linux (GCC y Clang, Debug y Release, C++17 y C++20), macOS, Windows (MSVC), ambos sanitizers y el formato.

## Novedades

Consulte [CHANGELOG.md](CHANGELOG.md). El código escrito para la 1.0 (`#include "adaptive_allocator.hpp"`, el
constructor `allocation_telemetry(idle_ms, window_ms, threshold)`, `compute_capacity`, `record_insertion`) sigue
compilando sin cambios.

## Licencia

MIT — véase [LICENSE](LICENSE).

---

Anton Sergeev — avsergeev1981@gmail.com
