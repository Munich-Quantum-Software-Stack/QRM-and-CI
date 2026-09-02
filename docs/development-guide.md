# Development Guide - Working on QRM&CI

This guide is for contributors changing QRM&CI itself. It covers the repository layout, the shipped
deployment modes, the local build/test loop, and the documentation and container-build rules that
keep the tree healthy.

If you only want to deploy the shipped daemons, use [Getting Started](getting-started.md). If you
want to add your own entrypoint against `qrmci`, use
[Build custom QRM&CI workflow](using-the-library.md).

## Repository layout

- `include/qrmci/` - public interfaces
- `src/` - the `qrmci` library implementation
- `apps/standalone/` - the standalone daemon entrypoint, Dockerfile, and example config
- `apps/distributed/` - the selector and worker entrypoints, Dockerfile, bake file, and example
  configs
- `tests/unit/` - the GoogleTest-based unit targets
- `tests/integration/` - client-only integration binaries for a live external deployment
- `docs/` - the Doxygen main page and hand-written documentation pages
- `cmake/` - helper modules and pinned dependency versions

## QRM&CI Library

### Component responsibilities

| Surface                                               | Responsibility                                                                               |
| ----------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| `Config.h` / `Config.cpp`                             | configuration model, file discovery, defaults, and documented environment-variable overrides |
| `CommunicationHandler.h` / `CommunicationHandler.cpp` | RabbitMQ send/receive wrappers using `std::expected` and manual acknowledgement              |
| `BackendWrapper.h` / `BackendWrapper.cpp`             | backend capability snapshots from a live submitter or a published status message             |
| `BackendRegistry.h` / `BackendRegistry.cpp`           | per-process backend liveness, deterministic iteration order, and publication pacing          |
| `ConstantsMapping.h` / `ConstantsMapping.cpp`         | pure translations between QDMI, mqss protocol, and mqss-ci enumerations                      |
| `Runners.h` / `Runners.cpp`                           | selection, compilation, submission, result collection, and cancellation helpers              |
| `Error.h` / `Error.cpp`                               | structured error kinds plus human-readable detail                                            |
| `Logger.h`                                            | the shared console-plus-file logger helper                                                   |

## QRM&CI Application Workflow

### Standalone

For the concept-level model, start with [Overview](index.md); for the step-by-step composition, see
[Build custom QRM&CI workflow](using-the-library.md). What matters before you change
`apps/standalone/main.cpp` is _why_ its loop is ordered the way it is.

The daemon refreshes its own backend registry entry and expires stale entries **before** it receives
work, and the receive itself is bounded by `config.selector.taskReceiveTimeout` rather than blocking.
Together those two facts mean a quiet intake queue cannot starve the refresh and let the daemon
expire its own backend out from under itself. Any change that makes the receive unbounded, or that
moves it ahead of the refresh, breaks that invariant.

Ready jobs are executed one at a time through `executeQuantumTask(...)`, and the process exits
cleanly on `SIGINT` and `SIGTERM`.

### Distributed

Distributed mode keeps the same pipeline stages but splits them across two kinds of process — one
selector and one or more workers:

- `apps/distributed/selector/main.cpp` owns backend selection. It receives backend-status messages on
  `config.selector.backendStatusQueue`, receives tasks on `config.common.qrmciQueue`, runs
  `chooseBackend(...)`, and forwards the selected task to `config.compiler.queue`.
- `apps/distributed/worker/main.cpp` owns compilation, scheduling, submission, and result handling.
  It publishes its own backend status, compiles tasks from `config.compiler.queue`, schedules them,
  submits every ready job, then collects their results.

The worker's submit-all-then-collect ordering is the key behavioral difference from standalone: it
allows multiple jobs to be in flight on the device at once. The selector never opens a QDMI
connection, so every backend it can choose from comes from a worker's status message.

## Build and test workflow

### Development container

The repository ships a development container under `.devcontainer/`. Its Dockerfile installs the
toolchain and system packages QRM&CI needs — compiler, CMake, Ninja, `pre-commit`, ccache, and the
RabbitMQ/spdlog/protobuf development packages — and copies the prebuilt MQSS dependency tree from the
same `mqssci-deps` image CI uses into `/opt/deps`. The devcontainer definition also preconfigures the
VS Code C++/CMake/Doxygen extensions and the formatting settings this repository expects. Opening the
repository in VS Code and choosing _Reopen in Container_ is the quickest way to a working build
environment, and it is the recommended one.

The container does **not** carry credentials. The MQSS components are fetched from GitHub at
configure time, so `git` inside the container must be able to reach those repositories — forward your
credentials or SSH agent into the container yourself.

### Build and run the unit tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build/tests --output-on-failure
```

`ctest --test-dir build/tests` is the canonical invocation and matches what CI runs. Useful variants:

- run only the unit tests:

  ```bash
  ctest --test-dir build/tests/unit --output-on-failure
  ```

- run one CTest target:

  ```bash
  ctest --test-dir build/tests --output-on-failure -R TestRunnersBackendSelection
  ```

- reconfigure after toolchain or dependency changes:

  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  ```

  Re-running configure reuses the existing cache. If a dependency revision changed in
  `cmake/DependenciesVersion.cmake`, delete `build/` and configure fresh instead — the tree is
  disposable, and CI always builds from scratch.

- run the integration tests. These need a reachable RabbitMQ broker and a running QRM&CI deployment
  (standalone or distributed) configured against the same broker and queues:

  ```bash
  ctest --test-dir build/tests/integration --output-on-failure
  ```

  This runs every registered integration target — `submit_task`, `submit_task_distributed`,
  `submit_task_unsupported_backend`, `submit_task_compile_failure`, and `submit_task_concurrent`.
  Several of them only make sense against a particular deployment mode; `submit_task_distributed`,
  for instance, expects a selector-plus-worker deployment. Select a single target with `-R` when you
  are running against a single mode.

Notes:

- `QRMCI_BUILD_APPS` defaults to `ON`; disable it only if you do not need the daemon targets
- `QRMCI_BUILD_UNIT_TESTS` and `QRMCI_BUILD_INTEGRATION_TESTS` default to `ON`
- the unit tests `TestRunnersTaskExecution` and `TestBackendWrapper` need the QDMI environment that
  CTest injects for them, so run them through CTest rather than by invoking the binary directly
- the integration binaries under `tests/integration/` require a live external deployment and are not
  wired into CI

### Build options

| Option                          | Default | Meaning                                              |
| ------------------------------- | ------- | ---------------------------------------------------- |
| `QRMCI_BUILD_UNIT_TESTS`        | `ON`    | build the unit test targets                          |
| `QRMCI_BUILD_INTEGRATION_TESTS` | `ON`    | build the integration test targets                   |
| `QRMCI_BUILD_APPS`              | `ON`    | build the standalone and distributed daemon binaries |
| `BUILD_QRMCI_DOCS`              | `OFF`   | build the Doxygen `docs` target                      |
| `QRMCI_WARNINGS_AS_ERRORS`      | `OFF`   | treat QRM-owned target warnings as errors            |

Pinned dependency revisions live in `cmake/DependenciesVersion.cmake`.

## Quality checks

`pre-commit run -a` is the same check CI runs first; a failure there blocks the build and
static-analysis jobs. It covers formatting, spelling, license headers, and the capitalization checks
configured in `.pre-commit-config.yaml`.

```bash
pre-commit run -a
```

For static analysis, CI configures with compile commands and runs `run-clang-tidy` over everything
under `include/`, `src/`, `apps/`, and `tests/`. To reproduce it exactly:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
run-clang-tidy -p build "^$(pwd)/(include|src|apps|tests)/.*"
```

To check a single file while iterating, run `clang-tidy -p build src/Runners.cpp` instead.

## Documentation workflow

Build the docs target locally with:

```bash
cmake -S . -B build \
  -DBUILD_QRMCI_DOCS=ON \
  -DQRMCI_BUILD_UNIT_TESTS=OFF \
  -DQRMCI_BUILD_INTEGRATION_TESTS=OFF \
  -DQRMCI_BUILD_APPS=OFF
cmake --build build --target docs --parallel 2
```

Generated HTML lands at `build/docs/html/index.html`.

Two rules matter when you add or rename documentation pages:

1. add the new `docs/*.md` file to `docs/CMakeLists.txt`'s `QRMCI_DOXYGEN_INPUT_DIRS` list
2. add or update the matching tab in `docs/layout.xml`

The glob in `docs/CMakeLists.txt` already makes `docs/*.md` part of the build dependencies, but
`INPUT` is an explicit list. A page that is not added there silently does not build.

The docs are gated by `.github/workflows/publish-docs.yml`, which builds the `docs` target and fails
if `build/docs/doxygen_warnings.txt` is non-empty. That workflow runs on pushes to `develop`, not on
pull requests, so build the docs target locally before you open a PR — a dangling `@ref` or an
unlisted page will otherwise break `develop` after the merge.

## Container image internals

The standalone and distributed images are built from separate multi-stage Dockerfiles:

- `apps/standalone/Dockerfile` builds `qrmcid-standalone` in a `builder` stage, then copies the
  binary, `qrmci`, and its runtime libraries into a `runtime` stage
- `apps/distributed/Dockerfile` uses one shared `builder` stage, then emits `runtime-selector` and
  `runtime-worker` stages for the two distributed binaries

Build targets from the repository root:

```bash
make docker-build-standalone
make docker-build-distributed
make docker-build-distributed-selector
make docker-build-distributed-worker
```

The distributed images go through `apps/distributed/docker-bake.hcl` rather than a plain
`docker build`, because the two runtime stages share one builder stage: the `selector` and `worker`
bake targets select the `runtime-selector` and `runtime-worker` stages of the same Dockerfile, and
the default bake group builds both. The bake file reads `DOCKERFILE_DISTRIBUTED`,
`IMAGE_NAME_DISTRIBUTED_SELECTOR`, `IMAGE_NAME_DISTRIBUTED_WORKER`, and `IMAGE_TAG_DISTRIBUTED` from
the environment, which is why the `Makefile` exports the variables it overrides.

Image tags are not hardcoded. `IMAGE_TAG_STANDALONE` and `IMAGE_TAG_DISTRIBUTED` are extracted in the
`Makefile` from the `project(... VERSION x.y.z ...)` line of the matching `apps/*/CMakeLists.txt`, so
bumping an app's project version changes the tag the `docker-run-*` targets look for — rebuild the
image before running it.

## Contributor guidelines

- Keep public interfaces in `include/qrmci/` and implementation in `src/`.
- Preserve existing option names and build toggles.
- Update tests when you change behavior.
- Update the relevant docs when you change behavior, build options, or runtime configuration.
