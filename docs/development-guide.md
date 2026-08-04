# Development Guide

This guide is for contributors extending QRM&CI functionality, debugging workflow behavior, and maintaining documentation quality.

## Repository Layout

Key directories:

- include/: public interfaces used by QRM&CI daemon components
- src/: daemon implementation and workflow orchestration
- tests/: CTest targets and integration-style checks
- docs/: Doxygen main page and static documentation content
- cmake/: package find or build dependency modules

## Core Runtime Architecture

The daemon entrypoint is `src/main.cpp`. At startup it:

1. Loads and initializes configuration (`loadConfig`, `initConfig`).
2. Creates the process logger and MQSS messenger.
3. Instantiates scheduler and submitter components.
4. Enters a continuous loop that:
   - receives the next quantum task,
   - selects backend metadata,
   - compiles circuits,
   - schedules execution,
   - submits to QDMI,
   - and sends result payloads back.

Primary component responsibilities:

- `include/Config.hpp`, `src/Config.cpp`
  - Environment-backed configuration model and process-wide config lifecycle.
- `include/BackendSelector.hpp`, `src/BackendSelector.cpp`
  - Backend selection logic and target annotation.
- `include/Runners.hpp`, `src/Runners.cpp`
  - Compilation stage orchestration (`compile_quantum_task`).
- `include/Messaging.hpp`, `src/Messaging.cpp`
  - Transport and protocol glue for receiving tasks and publishing results.

## Build and Test Workflow

Standard local workflow:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
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

When adding public interfaces, prefer documenting declarations in `include/` so generated API pages stay complete.

## Contributor Guidelines

- Keep interfaces in `include/` and implementation details in `src/`.
- Preserve existing CMake option names and build toggles.
- Prefer small, focused commits touching one behavior at a time.
- Add or update tests in `tests/` for behavior changes.
- Ensure docs are updated alongside new user-facing behavior.

## Troubleshooting

### Configure Fails on Missing Packages

Symptoms:

- CMake fails to resolve MQSS or logging dependencies.

Checks:

- Confirm required packages are installed and discoverable by CMake.
- Verify `CMAKE_PREFIX_PATH` and custom package locations.
- Re-run configure after environment fixes.

### Docs Target Is Missing

Symptoms:

- `docs` target does not appear in build tooling.

Checks:

- Configure with `-DBUILD_QRMCI_DOCS=ON`.
- Confirm Doxygen is installed and visible in PATH.

### Tests Do Not Execute

Symptoms:

- `ctest` reports no tests or fails to locate binaries.

Checks:

- Build completed successfully before running CTest.
- Run from repository root using `ctest --test-dir build --output-on-failure`.
- Inspect test registration in `tests/CMakeLists.txt`.

### Runtime Queue/Connection Problems

Symptoms:

- Daemon waits indefinitely or cannot exchange messages.

Checks:

- Verify RabbitMQ environment variables (`QRM_AMQP_*`).
- Confirm queue naming variables (`QRM_*_QUEUE`) match your setup.
- Check logs written by configured logger paths from config.
