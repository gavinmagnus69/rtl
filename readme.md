# rtl

`rtl` is a modular C++20 utility library focused on reusable infrastructure:
thread pools, concurrent queues, allocators, JSON helpers, configuration and
logging utilities, middleware primitives, WebSocket infrastructure, and database
wrappers.

The project is designed as a collection of independently selectable CMake
modules, so consumers can enable only the parts they need.

## Current Status

`rtl` is under active development.

The STP module is the most mature part of the library. It currently includes
lifecycle-tested thread-pool behavior, bounded queue admission policies,
executor integration, graceful shutdown, join idempotence, and periodic task
exception handling.

Other modules are evolving and may still have incomplete CMake/export/install
polish. Public APIs can change before the first stable release.

## Modules

| Module | CMake option | Description |
| --- | --- | --- |
| STP | `WITH_STP_BUILD` | Runtime thread pool, executor abstraction, and MPMC queue utilities |
| JSON | `WITH_JSON_BUILD` | JSON helper utilities based on `nlohmann_json` |
| Allocators | `WITH_ALLOCATORS_BUILD` | Linear and block allocator utilities |
| Core | `WITH_CORE_BUILD` | Configuration and logging primitives |
| Middleware | `WITH_MIDDLEWARE_BUILD` | Authentication and authorization interfaces |
| WebSocket | `WITH_WS_BUILD` | Boost.Beast-based WebSocket server components |
| Database | `WITH_DB_BUILD` | PostgreSQL/libpqxx connection, pooling, and transaction wrappers |
| Tests | `WITH_TESTS_BUILD` | Feature-level tests |

Deprecated components such as `StaticThreadPool` and `UnboundedMPMCQueue` are
kept for compatibility only. New code should use `ThreadPool` and
`UnbMpMcTemplateQueue`.

## Requirements

- C++20 compiler
- CMake 3.27+
- Ninja recommended
- Optional dependency manager: vcpkg

External dependencies depend on enabled modules:

- `nlohmann_json`
- `spdlog`
- `libpqxx`
- `Boost`
- `zlib`

## Quick Start

Add the repository as a subdirectory and link the aggregate target:

```cmake
add_subdirectory(rtl)

target_link_libraries(my_app PRIVATE
  librtl::librtl
)
```

Or link a specific module:

```cmake
target_link_libraries(my_app PRIVATE
  librtlstp::librtlstp
)
```

## Configure

Minimal configuration without database or WebSocket modules:

```sh
cmake -S . -B build -G Ninja \
  -DWITH_DB_BUILD=OFF \
  -DWITH_WS_BUILD=OFF
```

With vcpkg:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

Build:

```sh
cmake --build build
```

## Thread Pool Example

```cpp
#include <RtlThreadPool.hpp>

int main() {
  rtl::stp::ThreadPool pool{4, 8};

  auto result = pool.put([] {
    return 42;
  });

  int value = result.get();

  pool.shutdown_graceful();
  pool.join();

  return value == 42 ? 0 : 1;
}
```

## STP Features

- Graceful shutdown that stops accepting new work and drains accepted tasks.
- Idempotent `join()`.
- Bounded queue support.
- Admission policies: throw, block, timed block, and caller-runs.
- Periodic task support.
- Executor abstraction via `IExecutor` and `ThreadPoolExecutor`.
- Typed thread-pool exceptions with error codes.

## Project Layout

```text
stp/          Thread pool, executor, and queue utilities
allocators/   Custom allocator utilities
json/         JSON helpers
core/         Config and logger modules
middleware/   Auth middleware interfaces
ws/           WebSocket server components
db/           Database core, pool, and transaction modules
3rdparty/     Dependency discovery/fetch logic
test/         Feature-level tests
cmake/        Shared CMake helpers
```

## Design Goals

- Clear module boundaries.
- Explicit dependency checks at configure time.
- RAII-safe resource ownership.
- Predictable shutdown semantics.
- Testable concurrency primitives.
- CMake targets suitable for downstream consumers.

## License

License information has not been added yet.
