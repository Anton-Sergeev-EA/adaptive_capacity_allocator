# Docker

## Сборка и запуск проекта с GCC и Clang

Образы собираются из корня репозитория (контекст — `.`). Тесты (`ctest`) выполняются
прямо на этапе сборки: если они не проходят, образ не соберётся.

```bash
docker build -f docker/Dockerfile.gcc   -t adaptive-allocator:gcc   .
docker build -f docker/Dockerfile.clang -t adaptive-allocator:clang .

docker run --rm adaptive-allocator:gcc                      # тесты + демонстрация
docker run --rm adaptive-allocator:clang
docker run --rm adaptive-allocator:clang tests              # только тесты
docker run --rm adaptive-allocator:gcc benchmark            # сравнение с std::vector
docker run --rm adaptive-allocator:gcc demo --lang en       # демонстрация на нужном языке
```

Образы основаны на Ubuntu 24.04 (GCC 13, Clang 18). Проекту нужны C++17
(`if constexpr`, `std::align_val_t`) и CMake 3.14 или новее.

## Минимальное окружение C++ на Ubuntu 16.04

Отдельный образ `docker/hello` — минимальная среда с GCC 5.4, nano и vim, которая собирает
консольную программу «Hello, world». Сам проект в нём не собирается: GCC 5.4 и CMake 3.5
слишком старые для C++17.

```bash
docker build -t hello-u1604 docker/hello
docker run --rm hello-u1604                 # -> Hello, world! (built with g++ 5.4.0 ...)
docker run --rm -it hello-u1604 bash        # внутри доступны g++, nano, vim
```

## Совместимость с Clang

Sized `operator delete(void*, size_t)` GCC включает по умолчанию, а Clang до 19-й версии —
только с `-fsized-deallocation`. Поэтому вызов в `include/adaptive/adaptive_allocator.hpp`
обёрнут в проверку `__cpp_sized_deallocation` с откатом на обычный `operator delete`.
