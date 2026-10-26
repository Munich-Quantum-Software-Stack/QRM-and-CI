# `tests/real-backend` — real IQM backend test

## What this is

`TestIQMWorker` is the one test in this repository that talks to a **real,
external quantum backend** — IQM's Resonance API — instead of the simulated
QDMI example driver every `tests/integration` target uses. Which specific
IQM device it targets is a runtime choice (see below); which API endpoint
it talks to is not, since talking to IQM's Resonance API is what this test
_is_.

Because it depends on network reachability to that real service, and on
real credentials, it is deliberately kept apart from the rest of the test
suite:

- `tests/real-backend/CMakeLists.txt` never calls `add_test()`. There is no
  `ctest` label, `-R` filter, or `-L` selection that can ever schedule this
  test — it simply does not exist as far as `ctest` is concerned.
- The whole directory is only configured when `QRMCI_BUILD_REAL_BACKEND_TESTS`
  is explicitly turned on (default `OFF`), mirroring how
  `QRMCI_BUILD_INTEGRATION_TESTS` gates `tests/integration` in
  `tests/CMakeLists.txt`.
- It is built and run only through this directory's own `run.sh`.
- Running it requires real RabbitMQ and IQM credentials, which `run.sh`
  insists on getting from your own shell environment rather than ever
  reading a committed default (see "Required environment variables" below).

## Directory contents

| Path                                 | Purpose                                                                      |
| ------------------------------------ | ---------------------------------------------------------------------------- |
| `CMakeLists.txt`                     | Defines the `TestIQMWorker` executable target. No `add_test()`.              |
| `TestIQMWorker.cpp`                  | The test itself (originally `tests/integration/TestTwoWorkerSimulator.cpp`). |
| `run.sh`                             | Validates required credentials, builds, and runs `TestIQMWorker`.            |
| `config/selector-test-config.toml`   | Config the spawned selector daemon loads.                                    |
| `config/worker-iqm-test-config.toml` | Config the spawned worker-iqm daemon loads.                                  |

The two `config/*.toml` files are local copies kept inside this directory
(rather than reused from `apps/distributed/config/`, which holds the
equivalents the simulated/ctest-driven integration tests use), so this
directory carries its own fixtures and doesn't reach outside itself for
them. They still carry placeholder values for the fields covered by the
required environment variables below — those placeholders are never used in
practice, since an environment variable always wins over the file, and
`run.sh` refuses to run at all if the real values aren't supplied.

## Required environment variables

`run.sh` fails immediately — before touching the build or spawning any
daemon — if any of these aren't already exported in your shell:

| Variable                            | Why it's required, not defaulted                                                                                                    |
| ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `QRMCI_AMQP_HOST`                   | Which RabbitMQ broker to use. No safe default — the committed TOML's value is a placeholder.                                        |
| `QRMCI_AMQP_USER`                   | RabbitMQ credential.                                                                                                                |
| `QRMCI_AMQP_PASSWORD`               | RabbitMQ credential.                                                                                                                |
| `QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN` | IQM API credential.                                                                                                                 |
| `QRMCI_SUBMITTER_QDMI_DEVICE_NAME`  | Which IQM device to open.                                                                                                           |
| `IQM_QC_ALIAS`                      | Which specific IQM device alias to request from the Resonance API — lets you target a different real device without editing source. |

These are all fields the underlying `mqss::qrmci::Config` loader already
supports as `defaults < TOML file < environment variable` overrides (see
"Config files" below) — `run.sh` doesn't invent a new mechanism, it just
refuses to proceed on the committed file's placeholder values for anything
credential-shaped or deployment-specific.

Export them before invoking the script, e.g.:

```sh
export QRMCI_AMQP_HOST=rabbitmq.example.internal
export QRMCI_AMQP_USER=...
export QRMCI_AMQP_PASSWORD=...
export QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN=...            # Token from IQM portal
export QRMCI_SUBMITTER_QDMI_DEVICE_NAME=...             # (e.g. garnet)
export IQM_QC_ALIAS=...                                 # (same as QRMCI_SUBMITTER_QDMI_DEVICE_NAME e.g. garnet)

./tests/real-backend/run.sh
```

Two related variables are deliberately **not** on this list:

- `QRMCI_SUBMITTER_QDMI_DEVICE_ID` — optional, not required. It only has to
  be internally consistent within one run of this test (see "Test
  workflow" below for why), so it has a safe fallback rather than a
  required export.
- `IQM_BASE_URL` — hardcoded, not configurable at all. Talking to IQM's
  Resonance API specifically is what `TestIQMWorker` tests; if you need a
  different provider or API endpoint, that's a different test, not a flag
  on this one.

## Building and running

```sh
# One-time (or whenever you reconfigure): opt in to building this directory.
cmake -S . -B build -DQRMCI_BUILD_REAL_BACKEND_TESTS=ON

# Export the required credentials (see above), then build + run:
./tests/real-backend/run.sh
```

`run.sh`:

1. Validates every required environment variable is set (`require_env`),
   exiting with a clear error naming the missing one if not.
2. Runs `cmake --build "$BUILD_DIR" --target TestIQMWorker` — so it always
   runs against freshly built sources, never a stale binary.
3. `export`s `QRMCI_CONFIG_FILE` for the `TestIQMWorker` process itself
   (see the environment-variable breakdown below for why this can't just be
   baked into the CMake build like the daemon-facing paths are).
4. `exec`s the built binary, so the script's exit code and any signals
   (e.g. Ctrl-C) pass straight through.

`BUILD_DIR` defaults to `<repo-root>/build` and can be overridden:
`BUILD_DIR=/other/path ./tests/real-backend/run.sh`.

## Test workflow

`TestIQMWorker.cpp`'s `main()`:

1. Spawns two daemon subprocesses via `DaemonProcess` (reused from
   `tests/integration/DaemonProcess.{h,cpp}` — a plain `posix_spawn`/`waitpid`
   wrapper with no GTest or qrmci dependency of its own, so reusing it adds
   no framework coupling):
   - `qrmcid-distributed-selector`
   - `qrmcid-distributed-worker` (configured here to act as the worker-iqm backend)
2. Sleeps 60s to give both daemons time to come up and publish their
   backend-status heartbeat to each other (the same readiness grace
   `TestSubmitTaskDistributed.cpp` uses, doubled here since two workers are
   each independently publishing on the same interval).
3. Calls `test()`, which:
   - Loads its own config via `mqss::qrmci::loadConfig()` (this is where the
     `QRMCI_CONFIG_FILE` env var `run.sh` exports gets read).
   - Builds one `QuantumTask`, restricted (`restricted_resource_names`) to
     the worker's backend ID (see `workerDeviceId()` below), carrying the
     `bell.quake` circuit fixture, 100 shots, optimisation level 1.
   - Sends it to `config.common.qrmciQueue` — the selector's front door.
   - Waits (30s budget, `kResultTimeout`) for a `QuantumResult` on the
     task's own result queue.
   - Asserts the result arrived, `execution_status()` is true, **and**
     `executed_qpu()` matches that same backend ID. That last check is the
     one that actually catches a wrong-worker delivery — a task picked up
     and compiled by the wrong worker would still come back "successful",
     just tagged with the wrong `executed_qpu`.
4. Stops both daemons, in reverse start order.

### Why the worker's backend ID has its own helper function

`workerDeviceId()` (`TestIQMWorker.cpp:42-45`) is a single source of truth
for the worker-iqm backend's advertised ID, read once from
`QRMCI_SUBMITTER_QDMI_DEVICE_ID` (falling back to `"worker-iqm"` if unset)
and used in two places that must agree with each other:

- the `EnvOverride` handed to the spawned worker daemon (`main()`, line 161)
- the task's `restricted_resource_names` and `executed_qpu()` check
  (`test()`, line 71)

Before this helper existed, both call sites hardcoded the literal
`"worker-iqm"` independently. That was fine as long as nothing overrode it,
but it meant an env-var override on one side could silently stop matching
the other — the daemon would advertise whatever `QRMCI_SUBMITTER_QDMI_DEVICE_ID`
says, while `test()` kept checking against the old literal, failing with
"Task was executed by the wrong worker" for reasons that have nothing to do
with routing actually being broken. Routing both call sites through one
function closes that gap. `iqmDeviceAlias()` (`TestIQMWorker.cpp:47-50`)
reads `IQM_QC_ALIAS` the same way, but has no second call site to stay
consistent with — it's required by `run.sh` precisely because there's no
safe value to fall back to here (see "Required environment variables").

## Environment variables

Three separate processes are involved — `TestIQMWorker` itself, plus the
selector and worker-iqm daemons it spawns as children — and each only ever
sees its own environment. Nothing below is redundant with anything else;
each entry answers the same kind of question ("which config file?", "which
backend?", "which credential?") for a different process.

### 1. Exported by the caller, validated by `run.sh` before anything else runs

See "Required environment variables" above for the full list and why each
one has no safe default. Because `run.sh` doesn't rebuild its own process
environment before `exec`-ing the binary, whatever you exported is exactly
what `TestIQMWorker` inherits — and, via `DaemonProcess::start()`'s
environ-inheritance (below), what both spawned daemons inherit too, without
`run.sh` or `TestIQMWorker.cpp` needing to forward any of them explicitly.

### 2. Set by `run.sh` itself, for the `TestIQMWorker` process

| Variable            | Value                              | Why                                                                                                                                                                                                                                                   |
| ------------------- | ---------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `QRMCI_CONFIG_FILE` | `config/selector-test-config.toml` | `src/Config.cpp`'s `loadConfig()` reads this directly from the process environment at runtime (`getEnvOr("QRMCI_CONFIG_FILE", "config/qrmci.toml")`) — there is no compile-time equivalent, so it has to be set here rather than in `CMakeLists.txt`. |

### 3. Baked into `CMakeLists.txt` as compile definitions

These are string constants `TestIQMWorker.cpp`'s own `main()` reads at
compile time. On their own they don't set anything in any process's actual
environment — they only become environment variables once `main()` hands
them to a child via an `EnvOverride` (group 4, below).

| Macro                                | Value                                                   | Forwarded to                                        |
| ------------------------------------ | ------------------------------------------------------- | --------------------------------------------------- |
| `QRMCI_DISTRIBUTED_SELECTOR_PATH`    | `$<TARGET_FILE:qrmcid-distributed-selector>`            | Selector `DaemonProcess`'s executable path          |
| `QRMCI_DISTRIBUTED_WORKER_PATH`      | `$<TARGET_FILE:qrmcid-distributed-worker>`              | worker-iqm `DaemonProcess`'s executable path        |
| `QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR` | `<build>/tests/real-backend/daemon-logs/selector`       | Selector's log directory                            |
| `QRMCI_DISTRIBUTED_WORKER_B_LOG_DIR` | `<build>/tests/real-backend/daemon-logs/worker-iqm`     | worker-iqm's log directory                          |
| `QRMCI_SELECTOR_TEST_CONFIG`         | `tests/real-backend/config/selector-test-config.toml`   | Selector child's own `QRMCI_CONFIG_FILE` override   |
| `QRMCI_WORKER_IQM_TEST_CONFIG`       | `tests/real-backend/config/worker-iqm-test-config.toml` | worker-iqm child's own `QRMCI_CONFIG_FILE` override |
| `QRMCI_WORKER_IQM_QDMI_CONF`         | `tests/input/iqm_qdmi.conf`                             | worker-iqm child's own `QDMI_CONF` override         |

Log directories are created at **runtime**, by `DaemonProcess::start()`
(`tests/integration/DaemonProcess.cpp`), not at configure or build time —
they won't exist on disk until the test actually runs and spawns a daemon.

### 4. Set by `TestIQMWorker.cpp`'s `main()`, only in the spawned child's own environment

Built as `EnvOverride`s passed to `DaemonProcess`, which layers them on top
of a copy of the parent's own environment before handing the result to
`posix_spawn()` (`tests/integration/DaemonProcess.cpp`) — so these only ever
exist inside that one child process, never in `TestIQMWorker`'s own
environment. (The parent's own exported credentials from group 1 still
reach this child too, for whichever of these keys isn't itself overridden
below — that's the same environ-inheritance mechanism.)

**Selector child** (`TestIQMWorker.cpp:149-153`):

| Variable            | Value                        | Why                                                  |
| ------------------- | ---------------------------- | ---------------------------------------------------- |
| `QRMCI_CONFIG_FILE` | `QRMCI_SELECTOR_TEST_CONFIG` | Tells the selector daemon which TOML config to load. |

**worker-iqm child** (`TestIQMWorker.cpp:157-166`):

| Variable                         | Value                                                                                                                    | Why                                                                                                                                                                                           |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `QRMCI_SUBMITTER_QDMI_DEVICE_ID` | `workerDeviceId()` — `QRMCI_SUBMITTER_QDMI_DEVICE_ID` from `TestIQMWorker`'s own environment, or `"worker-iqm"` if unset | The backend ID this worker advertises to the selector; must match the task's `restricted_resource_names`, which is why both read the same helper (see "Test workflow" above).                 |
| `QRMCI_COMPILER_QUEUE`           | `compiler.worker-iqm`                                                                                                    | This worker's own dispatch queue, so a task the selector assigns to worker-iqm lands somewhere only worker-iqm listens, not a queue every worker would share.                                 |
| `QDMI_CONF`                      | `QRMCI_WORKER_IQM_QDMI_CONF` (`tests/input/iqm_qdmi.conf`)                                                               | QDMI driver config this worker's QDMI client layer opens.                                                                                                                                     |
| `QRMCI_CONFIG_FILE`              | `QRMCI_WORKER_IQM_TEST_CONFIG`                                                                                           | This worker's own qrmci TOML config.                                                                                                                                                          |
| `IQM_BASE_URL`                   | `https://resonance.iqm.tech` (hardcoded literal)                                                                         | The real IQM Resonance API endpoint — part of what this test _is_, not a deployment knob. See "Required environment variables" for why this one stays hardcoded while `IQM_QC_ALIAS` doesn't. |
| `IQM_QC_ALIAS`                   | `iqmDeviceAlias()` — `IQM_QC_ALIAS` from `TestIQMWorker`'s own environment (required by `run.sh`, so always set here)    | The specific IQM quantum computer requested from that API.                                                                                                                                    |

## Config files (`.toml`)

`config/selector-test-config.toml` and `config/worker-iqm-test-config.toml`
are plain qrmci TOML configs, same schema as everywhere else in this repo.
Their committed values for RabbitMQ connection and QDMI credential fields
are placeholders only — `run.sh`'s required environment variables always
override them (`defaults < file < environment variable`), so the files
never need to (and must not) carry real credentials. For the schema itself
— every field, its default, and the full precedence rule — see, rather than
duplicating it here:

- [`docs/getting-started.md`](../../docs/getting-started.md), the
  "Runtime configuration" section — the authoritative field-by-field and
  env-var reference.
- [`apps/qrmci.full-reference.example.toml`](../../apps/qrmci.full-reference.example.toml)
  — a fully annotated example covering every section.
- `apps/distributed/config/qrmci-distributed-selector.example.toml` /
  `qrmci-distributed-worker.example.toml` — the shipped, maintained examples
  for each daemon role that this directory's own configs were derived from.

## Gotchas

- **Missing credentials fail before any build happens.** `run.sh` checks
  every required environment variable up front (`require_env`) and exits
  with a named error rather than letting a placeholder value from the TOML
  file silently reach a real daemon.
- **Rebuild after editing `CMakeLists.txt`.** A compile-definition change
  (e.g. a config path) has no effect on an already-built binary. `run.sh`
  always rebuilds first; if you invoke the binary any other way, rebuild
  manually first (`cmake --build build --target TestIQMWorker`).
- **Log directories are a runtime side effect**, not something `cmake
--build` creates — see the note under environment-variable group 3 above.
- `TestIQMWorker.cpp` still `#include`s `RunWithDaemons.h` even though it
  never calls `runWithDaemons()`/`RUN_ALL_TESTS()` — a leftover from this
  file's `tests/integration/TestTwoWorkerSimulator.cpp` origin. Harmless
  (nothing links against it since it's never instantiated), but a candidate
  for cleanup if this test is to be made fully framework-free.
- Never registered with `add_test()` anywhere — `ctest`, under any label or
  filter, cannot run this test. Only `tests/real-backend/run.sh` (or
  manually invoking the built binary with every required variable set) does.
