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
- `tests/integration/` - client-only integration binaries, each spawning and tearing down its own
  daemon(s) via `DaemonEnvironment`
- `docs/` - the Doxygen main page and hand-written documentation pages
- `cmake/` - helper modules and pinned dependency versions

## QRM&CI Library

### Component responsibilities

| Surface                                               | Responsibility                                                                                                                                                               |
| ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Config.h` / `Config.cpp`                             | configuration model, file discovery, defaults, and documented environment-variable overrides                                                                                 |
| `CommunicationHandler.h` / `CommunicationHandler.cpp` | RabbitMQ send/receive wrappers using `std::expected` and manual acknowledgement                                                                                              |
| `BackendWrapper.h` / `BackendWrapper.cpp`             | backend capability snapshots from a live device or a published backend-status message                                                                                        |
| `BackendRegistry.h` / `BackendRegistry.cpp`           | per-process backend liveness and deterministic iteration order                                                                                                               |
| `PublicationThrottle.h`                               | header-only status-publication pacing, unrelated to and independent of registry liveness                                                                                     |
| `ConstantsMapping.h` / `ConstantsMapping.cpp`         | pure translations between the submitter, mqss protocol, and mqss-ci enumerations                                                                                             |
| `CircuitFormatPolicy.h` / `CircuitFormatPolicy.cpp`   | task/compiler/backend circuit-format admission policy: direct vs. compiled, and the one compiler target a backend offers                                                     |
| `Runners.h` / `Runners.cpp`                           | selection, compilation, submission, result collection, and cancellation helpers                                                                                              |
| `Daemon.h` / `Daemon.cpp`                             | daemon bring-up, device opening, own-backend registration and refresh, compile-and-schedule, plus the termination flag and SIGINT/SIGTERM handlers every entrypoint installs |
| `DaemonMessaging.h`                                   | the queue I/O a work loop shares: receive, drain, best-effort send, failure and result replies                                                                               |
| `Error.h` / `Error.cpp`                               | structured error kinds plus human-readable detail                                                                                                                            |
| `Logger.h`                                            | the shared console-plus-file logger helper                                                                                                                                   |

## QRM&CI Application Workflow

### Standalone

For the concept-level model, start with [Overview](index.md); for the step-by-step composition, see
[Build custom QRM&CI workflow](using-the-library.md). What matters before you change
`apps/standalone/main.cpp` is _why_ its loop is ordered the way it is.

The daemon refreshes its own backend registry entry and expires stale entries **before** it receives
work, and the receive itself is a non-blocking drain (poll the queue empty, then move on) rather
than a bounded wait. Together those two facts mean a quiet intake queue cannot starve the refresh
and let the daemon expire its own backend out from under itself, and a burst of tasks is processed
in one turn instead of throttling to one task per `stagePollInterval`. Any change that makes the
receive block, or that moves it ahead of the refresh, breaks that invariant.

Ready jobs are executed one at a time through `executeQuantumTask(...)`, and the process exits
cleanly on `SIGINT` and `SIGTERM`.

### Distributed

Distributed mode keeps the same pipeline stages but splits them across two kinds of process — one
selector and one or more workers:

- `apps/distributed/selector/main.cpp` owns backend selection. Each turn it drains
  `config.selector.backendStatusQueue` into its registry, expires stale entries, then drains
  `config.common.qrmciQueue`, running `chooseBackend(...)` per task and forwarding each selected task
  to `config.compiler.queue`.
- `apps/distributed/worker/main.cpp` owns compilation, scheduling, submission, and result handling.
  It publishes its own backend status, takes one task per turn from `config.compiler.queue` through
  `compileAndSchedule(...)`, submits every ready job, then collects their results.

Two behavioral differences from standalone matter. The worker takes one task per turn rather than
draining, so jobs it has already scheduled get submitted without waiting for the intake queue to run
dry. And it submits every ready job before collecting any result, so several jobs can be in flight
on the device at once. The selector never opens a QDMI connection, so every backend it can choose
from comes from a worker's status message.

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

Never put a token into `QRMCI_MQSS_SUBMITTER_REPOSITORY` or any other fetched repository URL: CMake
cache variables land verbatim in `CMakeCache.txt`, and the value FetchContent uses to clone also ends
up in the fetched repository's `.git/config`, so an embedded token persists in the build tree in two
places. Authenticate instead with a process-scoped git config override, scoped to the `cmake`
invocation and never written to disk:

```bash
GIT_CONFIG_COUNT=1 \
GIT_CONFIG_KEY_0="http.https://github.com/.extraheader" \
GIT_CONFIG_VALUE_0="AUTHORIZATION: basic $(printf 'x-access-token:%s' "$GITHUB_TOKEN" | base64 -w0)" \
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

or configure a standard git credential helper (`git config --global credential.helper ...`) so `git`
itself resolves credentials outside of CMake entirely. If a token was ever placed in
`QRMCI_MQSS_SUBMITTER_REPOSITORY`, treat it as exposed: rotate it and discard any local build
directory that may have cached it.

### Build and run the unit tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build/tests --output-on-failure
```

`ctest --test-dir build/tests` runs every test target this build was configured for. CI configures
with `-DQRMCI_RUN_INTEGRATION_TESTS=ON` and runs the unit and integration labels as two separate
steps (`ctest --test-dir build/tests -LE integration` then `-L integration`) so failures in one are
easy to tell apart from the other; running the plain command locally covers the same tests. Useful
variants:

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

- run the integration tests. These need a reachable RabbitMQ broker and the real example QDMI
  driver/device (the same environment `TestRunnersTaskExecution` and `TestDaemon` need),
  and are registered `DISABLED` for `ctest` unless the configure step passed
  `-DQRMCI_RUN_INTEGRATION_TESTS=ON`. No deployment needs to be started by hand: each target owns
  its own daemon(s) via `DaemonEnvironment`/`DaemonProcess`
  (`tests/integration/DaemonEnvironment.*`, `tests/integration/DaemonProcess.*`), spawning them at
  the start of the run and tearing them down at the end.

  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQRMCI_RUN_INTEGRATION_TESTS=ON
  cmake --build build --parallel
  ctest --test-dir build/tests/integration --output-on-failure
  ```

  This runs every registered integration target — `TestSubmitTask`, `TestSubmitTaskDistributed`,
  `TestSubmitTaskUnsupportedBackend`, `TestSubmitTaskCompileFailure`, `TestSubmitTaskConcurrent`,
  `TestScheduleTask`, and `TestSubmitTaskMultiWorker`. `TestSubmitTaskDistributed` and
  `TestSubmitTaskMultiWorker` are the only ones that spawn the distributed selector+worker(s) instead
  of the standalone daemon (`TestSubmitTaskMultiWorker` spawns two workers, each with a distinct
  device ID and dispatch queue, to exercise routing between them); every other target spawns
  `qrmcid-standalone`. Select a single target with `-R`, or every integration target with
  `-L integration`. Each target has a 120s `TIMEOUT`; one that exceeds it (the spawned daemon
  crashed, a broker that never delivers) is reported as a `ctest` timeout rather than hanging --
  check the spawned daemon's own log under
  `build/tests/integration/daemon-logs/<target>/` first. Since every spawned daemon binds the same
  compiled-in queue names against the same broker, `tests/integration/CMakeLists.txt` gives all seven
  targets the same `RESOURCE_LOCK` so `ctest -j` never runs two of them at once.

Notes:

- `QRMCI_BUILD_APPS` defaults to `ON`; disable it only if you do not need the daemon targets --
  `tests/integration` needs the daemon binaries it builds and is skipped with a configure-time
  warning if `QRMCI_BUILD_APPS` is `OFF`
- `QRMCI_BUILD_UNIT_TESTS` and `QRMCI_BUILD_INTEGRATION_TESTS` default to `ON`
- the unit tests `TestRunnersTaskExecution` and `TestDaemon` need the QDMI environment that CTest
  injects for them (`QDMI_CONF`, `LD_LIBRARY_PATH`, and the driver path compile definition), so run
  them through CTest rather than by invoking the binaries directly
- the integration binaries under `tests/integration/` still need a reachable RabbitMQ broker and the
  real example QDMI driver/device; CI provisions both (a `rabbitmq` service container, reachable at
  `localhost:5672` because the tests run in a container that shares the runner's network namespace)
  and runs them as their own step

### Build options

| Option                          | Default | Meaning                                              |
| ------------------------------- | ------- | ---------------------------------------------------- |
| `QRMCI_BUILD_UNIT_TESTS`        | `ON`    | build the unit test targets                          |
| `QRMCI_BUILD_INTEGRATION_TESTS` | `ON`    | build the integration test targets                   |
| `QRMCI_RUN_INTEGRATION_TESTS`   | `OFF`   | register the integration test targets for ctest      |
| `QRMCI_BUILD_APPS`              | `ON`    | build the standalone and distributed daemon binaries |
| `BUILD_QRMCI_DOCS`              | `OFF`   | build the Doxygen `docs` target                      |
| `QRMCI_WARNINGS_AS_ERRORS`      | `OFF`   | treat QRM-owned target warnings as errors            |

`qrmci` links with `LINKER:--no-as-needed` (see the comment above that `target_link_options` call in
`CMakeLists.txt`) to work around an `--as-needed`-default linker dropping a shared library MQSSCI's
static archives still need. The configure step probes linker support for that flag with
`check_linker_flag()` first and only applies it when supported, so a linker that rejects GNU-style
`--no-as-needed` configures without it instead of failing; this is a capability check, not a claim
that QRM&CI is supported on non-Linux/non-GNU toolchains.

Pinned dependency revisions live in `cmake/DependenciesVersion.cmake`.

MQSSSubmitter (the CMake target `mqss_submitter`) lives in its own repository,
[MQSS-Submitter](https://github.com/Munich-Quantum-Software-Stack/MQSS-Submitter), fetched by
`cmake/FindMQSSSubmitter.cmake`. It is a plain `FetchContent` fetch: the library ships no
`install()`/package config, so there is no installed-package mode to prefer. The revision is pinned
by `QRMCI_MQSS_SUBMITTER_GIT_TAG` in `cmake/DependenciesVersion.cmake`; point that at a local
checkout's commit, or add a `SOURCE_DIR` to the `FetchContent_Declare`, to build against a working
copy.

MQSSSubmitter's own `cmake_minimum_required(VERSION 3.40)` raises the CMake version this project can
actually be configured with above the `3.25` its own `cmake_minimum_required` declares.

A deployment does not declare the driver's QDMI version anywhere: MQSSSubmitter detects it from the
driver, and a version this build cannot speak comes back as an `openDevice()` failure. The only
submitter timing knob is `SubmitterConfig::jobWaitTimeout` (`[submitter] jobWaitTimeout`, seconds,
file-only, default `0` = wait indefinitely).

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

Each Dockerfile's `builder` stage runs the same `cmake` configure that pulls the private
Munich-Quantum-Software-Stack dependencies (see `cmake/DependenciesVersion.cmake`) via FetchContent
over HTTPS. Unlike the development container described above, which forwards your host SSH agent,
these image builds run without any ambient credential, so the clone fails unless you pass one in.
Set `GITHUB_TOKEN` to a token with read access to those repositories and the `Makefile` forwards it
as a BuildKit secret (`--secret id=github_token,env=GITHUB_TOKEN`); the builder stage only uses it
to authenticate that one `git clone`/`cmake` invocation, never writing it to disk, so it can't end
up in an image layer:

```bash
GITHUB_TOKEN=<token> make docker-build-standalone
GITHUB_TOKEN=<token> make docker-build-distributed
```

Omitting `GITHUB_TOKEN` reproduces the original failure: `fatal: could not read Username for
'https://github.com': No such device or address`.

## Contributor guidelines

- Keep public interfaces in `include/qrmci/` and implementation in `src/`.
- Preserve existing option names and build toggles.
- Update tests when you change behavior.
- Update the relevant docs when you change behavior, build options, or runtime configuration.
