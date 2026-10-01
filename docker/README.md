# Docker: сборка gcc и clang

## Задание 1 — Ubuntu 16.04 + gcc + редактор, «Hello world»

```bash
docker build -t hello-u1604 docker/hello
docker run --rm hello-u1604                 # -> Hello, world! (built with g++ 5.4.0 ...)
docker run --rm -it hello-u1604 bash        # внутри доступны g++, nano, vim
```

## Задание 2 — проект в контейнерах с gcc и clang

Сборка запускается из корня репозитория (контекст — `.`). Тесты (`ctest`) выполняются
прямо на этапе сборки образа: если они не проходят, образ не соберётся.

```bash
docker build -f docker/Dockerfile.gcc   -t adaptive-allocator:gcc   .
docker build -f docker/Dockerfile.clang -t adaptive-allocator:clang .

docker run --rm adaptive-allocator:gcc                      # тесты + демонстрация
docker run --rm adaptive-allocator:clang
docker run --rm adaptive-allocator:clang tests              # только тесты
docker run --rm adaptive-allocator:gcc benchmark            # сравнение с std::vector
docker run --rm adaptive-allocator:gcc demo --lang en       # демонстрация на нужном языке
```

Почему проект собирается не на Ubuntu 16.04: там gcc 5.4 и CMake 3.5, а проекту нужны
C++17 (`if constexpr`, `std::align_val_t`) и CMake ≥ 3.14. Поэтому для задания 2 используется Ubuntu 24.04
(gcc 13, clang 18).

## Что пришлось исправить для clang

`adaptive_allocator.hpp` вызывал sized `operator delete(void*, size_t)`. GCC включает
sized deallocation по умолчанию, а Clang до 19-й версии — только с `-fsized-deallocation`,
поэтому под clang сборка падала с `no matching function for call to 'operator delete'`.
Вызов теперь обёрнут в проверку `__cpp_sized_deallocation`, с откатом на обычный `operator delete`.
