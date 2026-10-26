# `tests/real-backend` — real-backend tests

## Summary

`tests/real-backend/` holds tests that talk to a **real, external quantum
backend**, unlike `tests/integration`, which uses a simulated QDMI driver.
Each provider gets its own subdirectory — currently just `IQM/`
(`TestIQMWorker`, talking to IQM's Resonance API). `IQM_BASE_URL` is
hardcoded (talking to IQM _is_ what this test is); which IQM _device_ it
targets is a runtime choice.

Kept apart from the rest of the suite:

- `IQM/CMakeLists.txt` never calls `add_test()` — no `ctest` label/filter
  can ever schedule it.
- Only configured when `QRMCI_BUILD_REAL_BACKEND_TESTS=ON` (default `OFF`).
- Built and run only via `tests/real-backend/IQM/run.sh`.
- Requires real RabbitMQ/IQM credentials from your shell env, never a
  committed default (see "Required environment variables").

## Building and running

```sh
cmake -S . -B build -DQRMCI_BUILD_REAL_BACKEND_TESTS=ON   # one-time, from repo root

# export the required credentials (see below), then:
./tests/real-backend/IQM/run.sh
```

`run.sh`: validates required env vars → `cmake --build --target
TestIQMWorker` → exports `QRMCI_CONFIG_FILE` for `TestIQMWorker` itself
(runtime-only, no compile-time equivalent) → `exec`s the binary.

`BUILD_DIR` defaults to `<repo-root>/build`, override with
`BUILD_DIR=/other/path ./tests/real-backend/IQM/run.sh`.

A future sibling provider must update `run.sh`'s own path math by hand:
repo root is `${SCRIPT_DIR}/../../..`, the binary is at
`${BUILD_DIR}/tests/real-backend/<provider>/<Target>`. Neither is derived
from CMake, unlike everything in `CMakeLists.txt`.

## Directory contents

```
tests/real-backend/
├── CMakeLists.txt          # add_subdirectory(IQM) -- nothing else
├── real-backend.md         # this file
└── IQM/
    ├── CMakeLists.txt      # Defines the TestIQMWorker target. No add_test().
    ├── TestIQMWorker.cpp   # The test itself
    ├── run.sh              # Validates credentials, builds, runs TestIQMWorker
    └── config/
        ├── selector-test-config.toml    # Config the spawned selector loads
        └── worker-iqm-test-config.toml  # Config the spawned worker-iqm loads
```

The two `IQM/config/*.toml` files are local copies (not shared with
`apps/distributed/config/`), so `IQM/` is self-contained. Their credential
fields are placeholders only — env vars always win, and `run.sh` refuses
to run without the real values.

## Required environment variables

`run.sh` fails immediately, before any build or daemon spawn, if unset:

| Variable                            | Why required                                       |
| ----------------------------------- | -------------------------------------------------- |
| `QRMCI_AMQP_HOST`                   | RabbitMQ broker — no safe default.                 |
| `QRMCI_AMQP_USER`                   | RabbitMQ credential.                               |
| `QRMCI_AMQP_PASSWORD`               | RabbitMQ credential.                               |
| `QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN` | IQM API credential.                                |
| `QRMCI_SUBMITTER_QDMI_DEVICE_NAME`  | Which IQM device to open (e.g. `garnet`).          |
| `IQM_QC_ALIAS`                      | Which IQM device alias to request (e.g. `garnet`). |

These are ordinary `mqss::qrmci::Config` fields (`defaults < TOML file <
env var`, see "Config files") — `run.sh` just refuses to fall back to the
committed placeholders for anything credential-shaped.

```sh
export QRMCI_AMQP_HOST=rabbitmq.example.internal
export QRMCI_AMQP_USER=...
export QRMCI_AMQP_PASSWORD=...
export QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN=...   # from IQM portal
export QRMCI_SUBMITTER_QDMI_DEVICE_NAME=...    # e.g. garnet
export IQM_QC_ALIAS=...                        # same device, e.g. garnet

./tests/real-backend/IQM/run.sh
```

Not required: `QRMCI_SUBMITTER_QDMI_DEVICE_ID` (optional — only needs to be
internally consistent, see "Test workflow") and `IQM_BASE_URL` (hardcoded —
it's part of what this test _is_, not a deployment knob).

## Test workflow

`IQM/TestIQMWorker.cpp`'s `main()`:

1. Spawns `qrmcid-distributed-selector` and `qrmcid-distributed-worker`
   (as worker-iqm) via `DaemonProcess` (reused from
   `tests/integration/DaemonProcess.{h,cpp}` — plain, no GTest/qrmci tie).
2. Sleeps 60s for both to come up and publish their status heartbeat.
3. `test()`: loads its own config (`QRMCI_CONFIG_FILE`) → sends one
   `QuantumTask` restricted to the worker's backend ID, carrying
   `bell.quake`, to `config.common.qrmciQueue` → waits (30s) for the
   `QuantumResult` → asserts `execution_status()` _and_
   `executed_qpu()` matches that backend ID (the check that actually
   catches a wrong-worker delivery; `execution_status()` alone wouldn't).
4. Stops both daemons, reverse order.

**Why `workerDeviceId()` exists:** the worker's backend ID is used in two
places that must agree — the daemon's `EnvOverride` and `test()`'s
`restricted_resource_names`/`executed_qpu()` check. Both used to hardcode
`"worker-iqm"` independently; an override on one side could silently stop
matching the other. One shared helper (reads `QRMCI_SUBMITTER_QDMI_DEVICE_ID`,
falls back to `"worker-iqm"`) closes that gap. `iqmDeviceAlias()` reads
`IQM_QC_ALIAS` the same way, but has no second call site — it's required
by `run.sh` instead, since there's no safe fallback for it.

## Environment variables

Three processes, each seeing only its own environment:

1. **Caller-exported, validated by `run.sh`** — see "Required environment
   variables". Since `run.sh` doesn't rebuild its own environment before
   `exec`, these reach `TestIQMWorker` and (via environ-inheritance in
   `DaemonProcess::start()`) both spawned daemons, with nothing forwarding
   them explicitly.
2. **Set by `run.sh`, for `TestIQMWorker` itself:**

   | Variable            | Value                                  | Why                                                                                     |
   | ------------------- | -------------------------------------- | --------------------------------------------------------------------------------------- |
   | `QRMCI_CONFIG_FILE` | `IQM/config/selector-test-config.toml` | `loadConfig()` reads this from the process env at runtime — no compile-time equivalent. |

3. **Baked into `IQM/CMakeLists.txt`** (compile-time constants; only
   become env vars once forwarded via an `EnvOverride` in group 4):

   | Macro                                | Value                                        | Forwarded to                              |
   | ------------------------------------ | -------------------------------------------- | ----------------------------------------- |
   | `QRMCI_DISTRIBUTED_SELECTOR_PATH`    | `$<TARGET_FILE:qrmcid-distributed-selector>` | Selector's executable path                |
   | `QRMCI_DISTRIBUTED_WORKER_PATH`      | `$<TARGET_FILE:qrmcid-distributed-worker>`   | worker-iqm's executable path              |
   | `QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR` | `<build>/.../IQM/daemon-logs/selector`       | Selector's log dir                        |
   | `QRMCI_DISTRIBUTED_WORKER_B_LOG_DIR` | `<build>/.../IQM/daemon-logs/worker-iqm`     | worker-iqm's log dir                      |
   | `QRMCI_SELECTOR_TEST_CONFIG`         | `IQM/config/selector-test-config.toml`       | Selector's `QRMCI_CONFIG_FILE` override   |
   | `QRMCI_WORKER_IQM_TEST_CONFIG`       | `IQM/config/worker-iqm-test-config.toml`     | worker-iqm's `QRMCI_CONFIG_FILE` override |
   | `QRMCI_WORKER_IQM_QDMI_CONF`         | `tests/input/iqm_qdmi.conf`                  | worker-iqm's `QDMI_CONF` override         |

   These use `CMAKE_CURRENT_SOURCE_DIR`/`_BINARY_DIR`, so they auto-adjust
   to nesting depth — unlike `run.sh` (see "Building and running"). Log
   dirs are created at runtime by `DaemonProcess::start()`, not at build time.

4. **Set by `main()`, only in the spawned child's env** (layered onto a
   copy of the parent's own env by `DaemonProcess`):

   **Selector:**

   | Variable            | Value                        | Why                 |
   | ------------------- | ---------------------------- | ------------------- |
   | `QRMCI_CONFIG_FILE` | `QRMCI_SELECTOR_TEST_CONFIG` | Which TOML to load. |

   **worker-iqm:**

   | Variable                         | Value                                    | Why                                                |
   | -------------------------------- | ---------------------------------------- | -------------------------------------------------- |
   | `QRMCI_SUBMITTER_QDMI_DEVICE_ID` | `workerDeviceId()`                       | Must match the task's `restricted_resource_names`. |
   | `QRMCI_COMPILER_QUEUE`           | `compiler.worker-iqm`                    | worker-iqm's own dispatch queue.                   |
   | `QDMI_CONF`                      | `QRMCI_WORKER_IQM_QDMI_CONF`             | QDMI driver config.                                |
   | `QRMCI_CONFIG_FILE`              | `QRMCI_WORKER_IQM_TEST_CONFIG`           | This worker's qrmci config.                        |
   | `IQM_BASE_URL`                   | `https://resonance.iqm.tech` (hardcoded) | Part of what this test _is_.                       |
   | `IQM_QC_ALIAS`                   | `iqmDeviceAlias()`                       | Which IQM device to request.                       |

## Config files (`.toml`)

Same schema as the rest of the repo; credential fields are placeholders
only (env vars always win). For the schema itself, see:
[`docs/getting-started.md`](../../docs/getting-started.md)'s "Runtime
configuration", [`apps/qrmci.full-reference.example.toml`](../../apps/qrmci.full-reference.example.toml),
or `apps/distributed/config/qrmci-distributed-{selector,worker}.example.toml`
(what `IQM/config/`'s files were derived from).

## Gotchas

- Missing credentials fail before any build — `run.sh`'s `require_env`.
- Rebuild after editing `IQM/CMakeLists.txt`: a compile-definition change
  has no effect on an already-built binary.
- Daemon log dirs are a runtime side effect, not a build artifact.
- `TestIQMWorker.cpp` still `#include`s `RunWithDaemons.h` though it never
  calls `runWithDaemons()`/`RUN_ALL_TESTS()` — harmless leftover, candidate
  for cleanup.
- No `add_test()` anywhere — only `run.sh` or a manual invocation runs this.
- `run.sh`'s path math (`../../..`, `tests/real-backend/IQM/TestIQMWorker`)
  is manual, not CMake-derived — won't auto-adjust if this directory moves.
