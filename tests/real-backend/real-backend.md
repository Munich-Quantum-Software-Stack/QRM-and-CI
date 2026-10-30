# `tests/real-backend` — real-backend tests

## Summary

`tests/real-backend/` holds tests that talk to a **real, external quantum
backend**, unlike `tests/integration`, which uses a simulated QDMI driver.
Each provider gets its own subdirectory — currently `IQM/`
(`TestIQMWorker`, talking to IQM's Resonance API) and `LRZ/`
(`TestLRZWorker`, talking to LRZ's quantum portal). Each test hardcodes its
own provider's base URL (`IQM_BASE_URL`/`LRZ_HOST_URL`) — talking to that
provider _is_ what the test is; which _device_ it targets is a runtime
choice.

Kept apart from the rest of the suite:

- Neither `IQM/CMakeLists.txt` nor `LRZ/CMakeLists.txt` ever calls
  `add_test()` — no `ctest` label/filter can ever schedule either.
- Only configured when `QRMCI_BUILD_REAL_BACKEND_TESTS=ON` (default `OFF`).
- Built and run only via `tests/real-backend/IQM/run.sh` or
  `tests/real-backend/LRZ/run.sh`.
- Requires real RabbitMQ/provider credentials from your shell env, never a
  committed default (see "Required environment variables").

## Building and running

```sh
cmake -S . -B build -DQRMCI_BUILD_REAL_BACKEND_TESTS=ON   # one-time, from repo root

# export the required credentials (see below), then:
./tests/real-backend/IQM/run.sh
# or:
./tests/real-backend/LRZ/run.sh
```

`run.sh`: validates required env vars → `cmake --build --target
TestIQMWorker`/`TestLRZWorker` → exports `QRMCI_CONFIG_FILE` for the test
binary itself (runtime-only, no compile-time equivalent) → `exec`s the
binary.

`BUILD_DIR` defaults to `<repo-root>/build`, override with
`BUILD_DIR=/other/path ./tests/real-backend/<provider>/run.sh`.

Each provider's `run.sh` keeps its own independent copy of this path math
(`${SCRIPT_DIR}/../../..` for the repo root,
`${BUILD_DIR}/tests/real-backend/<provider>/<Target>` for the binary) — a
future sibling provider must do the same by hand, since neither script's
path math is derived from CMake, unlike everything in `CMakeLists.txt`.

## Directory contents

```
tests/real-backend/
├── CMakeLists.txt          # add_subdirectory(IQM), add_subdirectory(LRZ) -- nothing else
├── real-backend.md         # this file
├── IQM/
│   ├── CMakeLists.txt      # Defines the TestIQMWorker target. No add_test().
│   ├── TestIQMWorker.cpp   # The test itself
│   ├── run.sh              # Validates credentials, builds, runs TestIQMWorker
│   └── config/
│       ├── selector-test-config.toml    # Config the spawned selector loads
│       └── worker-iqm-test-config.toml  # Config the spawned worker-iqm loads
└── LRZ/
    ├── CMakeLists.txt      # Defines the TestLRZWorker target. No add_test().
    ├── TestLRZWorker.cpp   # The test itself
    ├── run.sh              # Validates credentials, builds, runs TestLRZWorker
    └── config/
        ├── selector-test-config.toml    # Config the spawned selector loads
        └── worker-lrz-test-config.toml  # Config the spawned worker-lrz loads
```

Each provider's `config/*.toml` files are local copies (not shared with
`apps/distributed/config/`, nor with the other provider's `config/`), so
`IQM/` and `LRZ/` are each self-contained. Their credential fields are
placeholders only — env vars always win, and `run.sh` refuses to run
without the real values.

## Required environment variables

`run.sh` fails immediately, before any build or daemon spawn, if unset.

Shared by both providers — ordinary RabbitMQ credentials, no safe default:

| Variable              | Why required                       |
| --------------------- | ---------------------------------- |
| `QRMCI_AMQP_HOST`     | RabbitMQ broker — no safe default. |
| `QRMCI_AMQP_USER`     | RabbitMQ credential.               |
| `QRMCI_AMQP_PASSWORD` | RabbitMQ credential.               |

Provider-specific:

| Variable                            | IQM                                             | LRZ                                           |
| ----------------------------------- | ----------------------------------------------- | --------------------------------------------- |
| `QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN` | IQM API credential.                             | LRZ portal API credential.                    |
| `QRMCI_SUBMITTER_QDMI_DEVICE_NAME`  | Which IQM device to open (e.g. `garnet`).       | Which LRZ device to open (e.g. `EQE1`).       |
| Device-request variable             | `IQM_QC_ALIAS` — which device alias to request. | `LRZ_QDMI_DEVICE_QPU` — which QPU to request. |

These are ordinary `mqss::qrmci::Config` fields (`defaults < TOML file <
env var`, see "Config files") — `run.sh` just refuses to fall back to the
committed placeholders for anything credential-shaped. The device-request
variable differs by more than name: `IQM_QC_ALIAS` is read directly by
`TestIQMWorker.cpp` (`iqmDeviceAlias()`), while `LRZ_QDMI_DEVICE_QPU` is
never read by `TestLRZWorker.cpp` at all — it reaches the LRZ QDMI device
only through inherited environment (see "Environment variables", group 1).

```sh
export QRMCI_AMQP_HOST=rabbitmq.example.internal
export QRMCI_AMQP_USER=...
export QRMCI_AMQP_PASSWORD=...

# IQM:
export QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN=...   # from IQM portal
export QRMCI_SUBMITTER_QDMI_DEVICE_NAME=...    # e.g. garnet
export IQM_QC_ALIAS=...                        # same device, e.g. garnet
./tests/real-backend/IQM/run.sh

# LRZ:
export QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN=...   # from the LRZ portal
export QRMCI_SUBMITTER_QDMI_DEVICE_NAME=...    # e.g. EQE1
export LRZ_QDMI_DEVICE_QPU=...                 # same device, e.g. EQE1
./tests/real-backend/LRZ/run.sh
```

Not required: `QRMCI_SUBMITTER_QDMI_DEVICE_ID` (optional — only needs to be
internally consistent, see "Test workflow") and each provider's hardcoded
base URL (`IQM_BASE_URL`/`LRZ_HOST_URL` — part of what the test _is_, not a
deployment knob).

## Test workflow

`IQM/TestIQMWorker.cpp` and `LRZ/TestLRZWorker.cpp` share one shape in
`main()` — one selector and one worker daemon:

1. Spawns `qrmcid-distributed-selector` and `qrmcid-distributed-worker`
   (as worker-iqm/worker-lrz) via `DaemonProcess` (reused from
   `tests/integration/DaemonProcess.{h,cpp}` — plain, no GTest/qrmci tie).
2. Sleeps 60s for both to come up and publish their status heartbeat.
3. `test()`: loads its own config (`QRMCI_CONFIG_FILE`) → sends one
   `QuantumTask` restricted to the worker's backend ID, carrying
   `bell.quake`, to `config.common.qrmciQueue` → waits for the
   `QuantumResult` → asserts `execution_status()` _and_
   `executed_qpu()` matches that backend ID (the check that actually
   catches a wrong-worker delivery; `execution_status()` alone wouldn't).
4. Stops both daemons, reverse order.

**Why `workerDeviceId()` exists:** the worker's backend ID is used in two
places that must agree — the daemon's `EnvOverride` and `test()`'s
`restricted_resource_names`/`executed_qpu()` check. Both used to hardcode
`"worker-iqm"`/`"worker-lrz"` independently; an override on one side could
silently stop matching the other. Each provider has one shared helper
(reads `QRMCI_SUBMITTER_QDMI_DEVICE_NAME`, falls back to `"worker-iqm"` or
`"worker-lrz"`) that closes that gap.

IQM has one extra piece with no LRZ equivalent: `iqmDeviceAlias()` reads
`IQM_QC_ALIAS` the same way `workerDeviceId()` does, but has no second
call site — it's required by `run.sh` instead, since there's no safe
fallback for it. LRZ's `LRZ_QDMI_DEVICE_QPU` plays the analogous
device-selection role but is never read inside `TestLRZWorker.cpp` at all
(see "Required environment variables").

## Environment variables

Three processes, each seeing only its own environment, in the same four
groups for both providers:

1. **Caller-exported, validated by `run.sh`** — see "Required environment
   variables". Since `run.sh` doesn't rebuild its own environment before
   `exec`, these reach the test binary and (via environ-inheritance in
   `DaemonProcess::start()`) both spawned daemons, with nothing forwarding
   them explicitly. This is how LRZ's `LRZ_QDMI_DEVICE_QPU` reaches the
   worker-lrz daemon — it is never set via an `EnvOverride`.
2. **Set by `run.sh`, for the test binary itself:**

   | Variable            | IQM value                              | LRZ value                              | Why                                                                                     |
   | ------------------- | -------------------------------------- | -------------------------------------- | --------------------------------------------------------------------------------------- |
   | `QRMCI_CONFIG_FILE` | `IQM/config/selector-test-config.toml` | `LRZ/config/selector-test-config.toml` | `loadConfig()` reads this from the process env at runtime — no compile-time equivalent. |

3. **Baked into `<provider>/CMakeLists.txt`** (compile-time constants;
   only become env vars once forwarded via an `EnvOverride` in group 4):

   | Macro                                | IQM value                                    | LRZ value                                | Forwarded to                            |
   | ------------------------------------ | -------------------------------------------- | ---------------------------------------- | --------------------------------------- |
   | `QRMCI_DISTRIBUTED_SELECTOR_PATH`    | `$<TARGET_FILE:qrmcid-distributed-selector>` | _(same — one shared selector binary)_    | Selector's executable path              |
   | `QRMCI_DISTRIBUTED_WORKER_PATH`      | `$<TARGET_FILE:qrmcid-distributed-worker>`   | _(same — one shared worker binary)_      | Worker's executable path                |
   | `QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR` | `<build>/.../IQM/daemon-logs/selector`       | `<build>/.../LRZ/daemon-logs/selector`   | Selector's log dir                      |
   | `QRMCI_DISTRIBUTED_WORKER_B_LOG_DIR` | `<build>/.../IQM/daemon-logs/worker-iqm`     | `<build>/.../LRZ/daemon-logs/worker-lrz` | Worker's log dir                        |
   | `QRMCI_SELECTOR_TEST_CONFIG`         | `IQM/config/selector-test-config.toml`       | `LRZ/config/selector-test-config.toml`   | Selector's `QRMCI_CONFIG_FILE` override |
   | `QRMCI_WORKER_{IQM,LRZ}_TEST_CONFIG` | `IQM/config/worker-iqm-test-config.toml`     | `LRZ/config/worker-lrz-test-config.toml` | Worker's `QRMCI_CONFIG_FILE` override   |
   | `QRMCI_WORKER_{IQM,LRZ}_QDMI_CONF`   | `tests/input/iqm_qdmi.conf`                  | `tests/input/lrz_qdmi.conf`              | Worker's `QDMI_CONF` override           |

   These use `CMAKE_CURRENT_SOURCE_DIR`/`_BINARY_DIR`, so they auto-adjust
   to nesting depth — unlike `run.sh` (see "Building and running"). Log
   dirs are created at runtime by `DaemonProcess::start()`, not at build
   time. The two binary-path macros resolve to the exact same value for
   both providers, since IQM and LRZ reuse the same selector/worker
   binaries — only the surrounding log dirs and config paths differ.

4. **Set by `main()`, only in the spawned child's env** (layered onto a
   copy of the parent's own env by `DaemonProcess`):

   **Selector** — identical macro name for both providers:

   | Variable            | Value                        | Why                 |
   | ------------------- | ---------------------------- | ------------------- |
   | `QRMCI_CONFIG_FILE` | `QRMCI_SELECTOR_TEST_CONFIG` | Which TOML to load. |

   **Worker:**

   | Variable                         | IQM value                                 | LRZ value                                    | Why                                                |
   | -------------------------------- | ----------------------------------------- | -------------------------------------------- | -------------------------------------------------- |
   | `QRMCI_SUBMITTER_QDMI_DEVICE_ID` | `workerDeviceId()`                        | `workerDeviceId()`                           | Must match the task's `restricted_resource_names`. |
   | `QRMCI_COMPILER_QUEUE`           | `compiler.worker-iqm`                     | `compiler.worker-lrz`                        | Worker's own dispatch queue.                       |
   | `QDMI_CONF`                      | `QRMCI_WORKER_IQM_QDMI_CONF`              | `QRMCI_WORKER_LRZ_QDMI_CONF`                 | QDMI driver config.                                |
   | `QRMCI_CONFIG_FILE`              | `QRMCI_WORKER_IQM_TEST_CONFIG`            | `QRMCI_WORKER_LRZ_TEST_CONFIG`               | This worker's qrmci config.                        |
   | Hardcoded base URL               | `IQM_BASE_URL=https://resonance.iqm.tech` | `LRZ_HOST_URL=https://portal.quantum.lrz.de` | Part of what this test _is_.                       |
   | Device-request override          | `IQM_QC_ALIAS=iqmDeviceAlias()`           | _(none — see "Test workflow")_               | Which device to request.                           |

## Config files (`.toml`)

Same schema as the rest of the repo; credential fields are placeholders
only (env vars always win). For the schema itself, see:
[`docs/getting-started.md`](../../docs/getting-started.md)'s "Runtime
configuration", [`apps/qrmci.full-reference.example.toml`](../../apps/qrmci.full-reference.example.toml),
or `apps/distributed/config/qrmci-distributed-{selector,worker}.example.toml`
(what both `IQM/config/`'s and `LRZ/config/`'s files were derived from).

## Gotchas

- Missing credentials fail before any build — `run.sh`'s `require_env`.
- Rebuild after editing `IQM/CMakeLists.txt` or `LRZ/CMakeLists.txt`: a
  compile-definition change has no effect on an already-built binary.
- Daemon log dirs are a runtime side effect, not a build artifact.
- `TestIQMWorker.cpp` still `#include`s `RunWithDaemons.h` though it never
  calls `runWithDaemons()`/`RUN_ALL_TESTS()` — harmless leftover, candidate
  for cleanup. `TestLRZWorker.cpp` does not have this leftover.
- No `add_test()` anywhere — only `run.sh` or a manual invocation runs
  either test.
- Each provider's `run.sh` path math (`../../..`,
  `tests/real-backend/<provider>/<Target>`) is its own manual copy, not
  CMake-derived — won't auto-adjust if that provider's directory moves,
  and fixing one script doesn't fix the other.
