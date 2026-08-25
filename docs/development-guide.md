# Development Guide

This guide is for contributors extending QRM&CI functionality, debugging workflow behavior, and maintaining documentation quality.

## Repository Layout

Key directories:

- apps/standalone/: standalone QRM&CI daemon entrypoint and executable definition
- include/qrmci/: public QRM&CI interfaces and generated configuration template
- src/: QRM&CI library implementation and workflow orchestration
- tests/: CTest registration and integration-style checks
- docs/: Doxygen main page and static documentation content
- cmake/: package find or build dependency modules

## QRM&CI Standalone Architecture

The QRM&CI standalone daemon entrypoint is `apps/standalone/main.cpp`. At startup it:

1. Loads and initializes configuration (`loadConfig`, `initConfig`).
2. Creates the process logger and RabbitMQ communication handler.
3. Instantiates scheduler and submitter components.
4. Enters a continuous loop that:

- receives the next quantum task from the configured QRM&CI queue,
- selects an available backend,
- compiles the task,
- schedules ready jobs by priority,
- submits jobs through QDMI,
- and sends execution or cancellation results back.

The loop polls for work and exits cleanly when it receives `SIGINT` or
`SIGTERM`.

Primary component responsibilities:

- `include/qrmci/Config.h`, `src/Config.cpp`
  - Environment-backed configuration model and process-wide config lifecycle.
- `include/qrmci/BackendWrapper.h`, `src/BackendWrapper.cpp`
  - Backend metadata wrapper.
- `include/qrmci/CommunicationHandler.h`, `src/CommunicationHandler.cpp`
  - RabbitMQ transport and message handling.
- `include/qrmci/Runners.h`, `src/Runners.cpp`
  - Backend selection, compilation, and execution orchestration.
- `include/qrmci/Logger.h`
  - Process and component logger construction.

## Build and Test Workflow

Standard local workflow. Treat `build/` as disposable output; CI always uses a
fresh directory and does not depend on an existing local build.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

QRM&CI requires CMake 3.20 or newer and C++23. The supported compiler
baseline is GCC 13 or newer and Clang 17 or newer on Linux. CI currently
tests GCC 13, GCC 14, Clang 17, and Clang 18 on Ubuntu 24.04.

### Build Options

- `QRMCI_BUILD_UNIT_TESTS` (default `ON`): build the unit test targets.
- `QRMCI_BUILD_INTEGRATION_TESTS` (default `ON`): build the integration test targets.
- `QRMCI_BUILD_APPS` (default `ON`): build the standalone daemon.
- `BUILD_QRMCI_DOCS` (default `OFF`): build the Doxygen `docs` target.
- `QRMCI_WARNINGS_AS_ERRORS` (default `OFF`): treat QRM-owned target warnings as errors.

### Dependency Pinning

FetchContent revisions are centralized in `cmake/DependenciesVersion.cmake`.

To run the local formatting and analysis checks:

```bash
pre-commit run -a
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build \
  src/CommunicationHandler.cpp src/Config.cpp src/Runners.cpp \
  src/BackendWrapper.cpp apps/standalone/main.cpp \
  tests/integration/submit_task.cpp
```

Useful variants:

- Debug build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

- Reconfigure after dependency/toolchain changes:

```bash
cmake -S . -B build
```

## Documentation Workflow

To rebuild docs locally:

```bash
cmake -S . -B build -DBUILD_QRMCI_DOCS=ON
cmake --build build --target docs
```

Output:

- build/docs/html/index.html

When adding public interfaces, prefer documenting declarations in `include/qrmci/` so generated API pages stay complete.

## Contributor Guidelines

- Keep interfaces in `include/` and implementation details in `src/`.
- Preserve existing CMake option names and build toggles.
- Prefer small, focused commits touching one behavior at a time.
- Add or update tests in `tests/` for behavior changes.
- Ensure docs are updated alongside new user-facing behavior.
