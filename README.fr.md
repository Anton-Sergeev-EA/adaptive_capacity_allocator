# Allocateur de capacité adaptatif

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · **Français** · [Deutsch](README.de.md) · [Italiano](README.it.md)

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` est un conteneur C++17 doté de l'interface de `std::vector` qui **décide lui-même de combien
grandir**, d'après le débit d'insertion mesuré. Sous un flux dense d'insertions, il économise la mémoire ; avec des
insertions rares, il grandit aussi vite qu'un `std::vector` ordinaire.

**[Démonstration interactive dans le navigateur →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)** (8 langues)

## Pourquoi

Quand `std::vector` manque de place, il grandit toujours du même facteur : 2× avec GCC et Clang, 1,5× avec MSVC.
C'est rapide, mais juste après un doublement, jusqu'à la moitié de la mémoire réservée reste inutilisée. Pour les
services qui gardent des millions de tampons en mémoire (files de messages, journaux, télémétrie, carnets
d'ordres), ces moitiés vides finissent par représenter des gigaoctets.

`adaptive_vector` mesure à quel rythme les éléments sont insérés et choisit le facteur de croissance selon la charge :

| Débit d'insertion | Stratégie | Croissance |
|---|---|:---:|
| jusqu'à 100/s, la première insertion après une inactivité, tampons de moins de 1 024 éléments | agressive | ×2,0 |
| de 100 à 1 000/s | modérée | ×1,5 |
| plus de 1 000/s | conservatrice | ×1,1 |

Tous les seuils et facteurs se règlent via `growth_policy`.

## Chiffres

Linux x86_64, GCC 13.3, `-O3`, 1 000 000 d'insertions d'`int`, médiane de 5 exécutions (`acalloc bench`) :

| | Temps | Marge mémoire moyenne pendant le remplissage | Réallocations |
|---|:---:|:---:|:---:|
| `std::vector<int>` | ~4,4 ms | 36,4 % | 21 |
| `adaptive_vector<int>` 2.0 | ~8,8 ms | **5,0 %** | 81 |
| `adaptive_vector<int>` 1.0 | ~40 ms | 4,9 % | 129 |

**Le prix est bien réel.** Grandir de 10 % au lieu de doubler demande plus de réallocations et de copies : sur un
flux continu, le conteneur est environ 2 fois plus lent que `std::vector`. En contrepartie, la marge mémoire
moyenne tourne autour de 5 % au lieu de 35 à 50 %. Si, pour votre charge, la mémoire compte plus que le débit,
l'échange est rentable ; sinon, gardez `std::vector`. La capacité finale en un point donné dépend de l'endroit où
le remplissage s'arrête : à exactement 1 000 000 d'éléments, `std::vector` tombe sur son meilleur cas (2²⁰). La
comparaison honnête porte donc sur la marge moyenne pendant tout le remplissage, pas sur un seul point final.

Mesurez-le sur votre machine : le résultat dépend du processeur, de la mémoire et du compilateur.

```bash
./build/acalloc bench
```

## Démarrage rapide

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Ou après installation (`cmake --install build`) :

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

Ou copiez simplement [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) dans votre
projet : il ne dépend que de la bibliothèque standard.

### Utilisation

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "capacité : " << s.capacity
              << ", réallocations : " << s.reallocations
              << ", stratégie : " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### Politique de croissance personnalisée

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // plus doux que 1.1
p.high_threshold = 50'000;                        // conservatrice seulement au-delà de 50 000 insertions/s
p.idle_reset = std::chrono::milliseconds(500);    // inactivité = pause de plus de 0,5 s
adaptive::adaptive_vector<Order> book(p);
```

### Une télémétrie pour plusieurs conteneurs

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // décisions selon le débit cumulé, sans verrou
```

## Démonstration dans le terminal en 8 langues

Le programme `acalloc` montre le comportement du conteneur dans quatre scénarios : flux continu, rafales, filet
lent et flux avec pause. La langue est détectée d'après les paramètres régionaux du système.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # démonstration
./build/acalloc bench           # comparaison avec std::vector
./build/acalloc languages       # liste des langues
./build/acalloc --lang fr       # interface en français
```

Langues prises en charge : Русский (principale), English, 中文, हिन्दी, Español, Français, Deutsch, Italiano.
La langue peut aussi être définie par la variable d'environnement `ACALLOC_LANG`. Pour ajouter une langue,
voir [docs/TRANSLATING.md](docs/TRANSLATING.md).

## API

| Type | Rôle |
|---|---|
| `adaptive_vector<T>` | Conteneur avec l'interface de `std::vector` : `push_back`, `emplace_back`, `insert`, `emplace`, `erase`, `resize`, `assign`, `reserve`, itérateurs, comparaisons, `swap`, ainsi que `stats()` et `telemetry()` |
| `growth_policy` | Facteurs de croissance, seuils de débit, fenêtre de mesure, seuil d'inactivité, capacité minimale pour l'adaptation |
| `allocation_telemetry` | Compteur de débit d'insertion sûr entre threads ; partageable entre conteneurs et threads |
| `adaptive_allocator<T>` | Allocateur standard qui prend en charge les types sur-alignés (`alignas(64)`, etc.) |

Sous le capot :

- **Aucun thread en arrière-plan.** La version 1.0 lançait un thread par vecteur ; 10 000 vecteurs signifiaient
  10 000 threads.
- **L'horloge n'est pas lue à chaque insertion.** Le conteneur compte les insertions localement et les transmet à
  la télémétrie toutes les 64 insertions et à chaque décision de croissance. Le chemin rapide de `push_back` se
  résume à un compteur et deux comparaisons.
- **L'inactivité est détectée.** Si aucune insertion n'a eu lieu pendant plus de `idle_reset`, les anciennes
  statistiques sont oubliées et la première insertion après la pause grandit de façon agressive.

## Compilation, tests et vérifications

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizers (ASan et TSan ne peuvent pas être liés dans un même binaire, d'où un choix entre trois options) :

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

Les compilations Docker avec GCC et Clang sont décrites dans [docker/README.md](docker/README.md).
La CI couvre Linux (GCC et Clang, Debug et Release, C++17 et C++20), macOS, Windows (MSVC), les deux sanitizers
et le formatage.

## Nouveautés

Voir [CHANGELOG.md](CHANGELOG.md). Le code écrit pour la 1.0 (`#include "adaptive_allocator.hpp"`, le constructeur
`allocation_telemetry(idle_ms, window_ms, threshold)`, `compute_capacity`, `record_insertion`) continue de
compiler sans modification.

## Licence

MIT — voir [LICENSE](LICENSE).

---

Anton Sergeev — avsergeev1981@gmail.com
