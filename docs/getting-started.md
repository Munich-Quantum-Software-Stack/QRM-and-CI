# Getting Started - Deploying QRM&CI

This guide is for deploying the shipped QRM&CI daemons. It takes you from choosing a deployment mode
to starting the process, configuring it, and checking that tasks flow through it.

If you need to change the pipeline itself, see
[Build custom QRM&CI workflow](using-the-library.md). If you need to work on QRM&CI as a contributor,
see [Development Guide](development-guide.md).

## Choose your deployment

| Deployment           | Source                      | Use it when...                                                                  |
| -------------------- | --------------------------- | ------------------------------------------------------------------------------- |
| Standalone           | `apps/standalone`           | one process should run the full pipeline                                        |
| Distributed selector | `apps/distributed/selector` | backend selection should run separately from device access                      |
| Distributed worker   | `apps/distributed/worker`   | compilation, scheduling, and submission should run in the device-owning process |

In distributed mode, one selector is paired with one or more workers. Each worker publishes its own
backend status, and the selector chooses among the backends it has heard from.

## Prerequisites

### Runtime prerequisites

- A RabbitMQ broker that is reachable from every process you start.
- For the standalone daemon and the distributed worker, access to a QDMI driver and device at
  runtime.
- A writable log directory, or an override for `QRMCI_LOG_DIR`.
- A configuration source: a TOML file, environment variables, or both.

### Build prerequisites

- CMake 3.25 or newer
- A C++23-capable compiler
- `MQSS Integration and Deployment Framework`, `MQSS Quantum Compilation Suite`, `MQSS Scheduler`, and
  `MQSS Submitter`
- The system packages listed in the two Dockerfiles, `apps/standalone/Dockerfile` and
  `apps/distributed/Dockerfile`

You do not install the MQSS components yourself: the `cmake/Find*.cmake` modules fetch them from
GitHub at configure time, at the revisions pinned in `cmake/DependenciesVersion.cmake`. Both build
paths therefore need network access, and `git` must be able to reach the MQSS repositories from the
build environment. The first configure is correspondingly long.

There are two ways to build:

1. **Path A** - build the container images from `apps/standalone/Dockerfile` and
   `apps/distributed/Dockerfile`.
2. **Path B** - build from source in a local environment that provides the toolchain and the system
   packages.

## Path A: run the container images

No prebuilt QRM&CI images are published, so build the images from this repository first. The
`Makefile` provides a build target for each image:

```bash
# To build Docker image with the standalone daemon
make docker-build-standalone
# To build Docker images for both selector and worker
make docker-build-distributed
# To build Docker image for selector only
make docker-build-distributed-selector
# To build Docker image for worker only
make docker-build-distributed-worker
```

### Run the daemons

The containers below need a reachable RabbitMQ broker. The `Makefile` defines one run target per
shipped image:

#### Standalone

```bash
make docker-run-standalone
```

#### Distributed

Start the selector and the workers as separate containers:

```bash
make docker-run-distributed-selector
make docker-run-distributed-worker
```

Each `docker-run-*` target is a convenience wrapper around `docker run --rm -it <image>:<tag>` for an
image that already exists locally. None of them mounts a config file or passes environment overrides,
so the container starts on the compiled-in defaults (see "Selected defaults" below) —
which is almost never what you want. For a real deployment, run `docker run` yourself with
`-e QRMCI_CONFIG_FILE=...` and a mounted config file, or with the individual `QRMCI_*` overrides.

## Path B: build from source

From the repository root, configure and build into a disposable `build/` directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

For a deploy-focused source build, you can keep the application targets enabled and disable the test
targets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DQRMCI_BUILD_UNIT_TESTS=OFF \
  -DQRMCI_BUILD_INTEGRATION_TESTS=OFF
cmake --build build --parallel
```

This skips building and running the tests; GTest must still be discoverable at configure time,
because the top-level `CMakeLists.txt` requires it unconditionally.

The shipped binaries are:

- `build/apps/standalone/qrmcid-standalone`
- `build/apps/distributed/qrmcid-distributed-selector`
- `build/apps/distributed/qrmcid-distributed-worker`

Relevant build options for a deployer:

- `QRMCI_BUILD_APPS` (default `ON`) controls whether the daemon binaries are built
- `QRMCI_BUILD_UNIT_TESTS` (default `ON`) controls the unit test targets
- `QRMCI_BUILD_INTEGRATION_TESTS` (default `ON`) controls the integration test targets

### Run the daemons

The commands below require a RabbitMQ and QDMI environment that matches your configuration.

#### Standalone

```bash
QRMCI_CONFIG_FILE=path/to/qrmci.toml build/apps/standalone/qrmcid-standalone
```

#### Distributed

Start the selector and worker in separate terminals:

```bash
QRMCI_CONFIG_FILE=path/to/qrmci-distributed-selector.toml \
  build/apps/distributed/qrmcid-distributed-selector
```

```bash
QRMCI_CONFIG_FILE=path/to/qrmci-distributed-worker.toml \
  build/apps/distributed/qrmcid-distributed-worker
```

## Stopping a daemon

All three shipped daemons install handlers for `SIGINT` and `SIGTERM`: the signal sets a termination
flag, the current loop turn finishes, and the process exits with status 0. This applies to both
paths. It is also why the containers stop cleanly — `docker stop` sends `SIGTERM`, and `Ctrl+C` in a
`docker run --rm -it` session sends `SIGINT`.

## Runtime configuration

QRM&CI assembles configuration in three layers, from lowest to highest precedence:

1. compiled-in defaults from `include/qrmci/ConfigDefaults.h.in` and the in-class defaults in
   `include/qrmci/Config.h`
2. an optional TOML file. QRM&CI reads the path in `QRMCI_CONFIG_FILE` if that variable is set;
   otherwise it looks for `config/qrmci.toml` **relative to the process's working directory**.
   Because that path is relative, a daemon started by systemd or inside a container will not find a
   config file unless you set `QRMCI_CONFIG_FILE` or control the working directory. A missing file is
   not an error — the daemon runs on defaults plus environment. A file that exists but does not parse
   is a startup error.
3. environment variables

### TOML files

Copy the matching example file, edit it for your environment, and point `QRMCI_CONFIG_FILE` at the
copy you want to use.

| Binary                        | Example file                                                      | What the process reads                                                                                                                                                 |
| ----------------------------- | ----------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `qrmcid-standalone`           | `apps/standalone/config/qrmci-standalone.example.toml`            | `[common]` for RabbitMQ, logs, task/result/scheduler queues, and poll interval; `[submitter]` for QDMI settings and backend registry timing                            |
| `qrmcid-distributed-selector` | `apps/distributed/config/qrmci-distributed-selector.example.toml` | `[common]`; `[selector]` for backend status queue, selector logging, registry timing, and receive timeouts; `[compiler].queue` as the forwarding target                |
| `qrmcid-distributed-worker`   | `apps/distributed/config/qrmci-distributed-worker.example.toml`   | `[common]`; `[selector].backendStatusQueue` as the publish target; `[compiler]` as the inbound task queue; `[submitter]` for QDMI settings and backend registry timing |

Keep credentials out of the TOML file when you can. Supply deployment-specific secrets through
environment variables at runtime instead.

### Environment variables

The documented runtime overrides in `src/Config.cpp` are:

- `QRMCI_CONFIG_FILE`
- `QRMCI_AMQP_HOST`
- `QRMCI_AMQP_PORT`
- `QRMCI_AMQP_USER`
- `QRMCI_AMQP_PASSWORD`
- `QRMCI_AMQP_VHOST`
- `QRMCI_QRMCI_QUEUE`
- `QRMCI_SCHEDULER_QUEUE`
- `QRMCI_COMPILER_QUEUE`
- `QRMCI_RESULTS_QUEUE`
- `QRMCI_SUBMITTER_QUEUE`
- `QRMCI_BACKEND_STATUS_QUEUE`
- `QRMCI_LOG_DIR`
- `QRMCI_SUBMITTER_QDMI_DRIVER_NAME`
- `QRMCI_SUBMITTER_QDMI_DEVICE_NAME`
- `QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN`

Receive timeouts, `common.stagePollInterval`, and backend-registry timing are file-only settings in
the current codebase.

### Selected defaults

The compiled-in defaults are development values, not deployment values. If you do not override them:

- RabbitMQ host `host.docker.internal`, port `5672`, vhost `/`
- RabbitMQ credentials `guest` / `guest`
- log directory `/var/log/qrmci`
- task queue `qrmci.tasks.queue`
- compiler queue `compiler.tasks.queue`
- scheduler queue `scheduler.tasks.queue`
- submitter queue `submitter.tasks.queue`
- backend status queue `backend.status.queue`
- results queue `test.results.queue`
- QDMI driver `qdmi_example_driver`, device `C++ Device with 5 qubits`, client token `token`

Two of those defaults matter most. The credentials are RabbitMQ's stock `guest`/`guest`, and the QDMI
target is the _example_ driver — so an unconfigured daemon starts happily against an example device
rather than refusing to start. The name of the default results queue, `test.results.queue`, is a fair
summary of the whole set. Override at minimum the connection settings, the credentials, the QDMI
driver/device, and the queue names before any real deployment.

## Confirm the daemon started

Each daemon logs to the console and, when the log directory is writable, to `daemon.log` under
`QRMCI_LOG_DIR` (default `/var/log/qrmci`). Watch for, in order:

1. no configuration error — a malformed TOML file makes the process log
   `Failed to load configuration: ...` and exit with status 1
2. the backends the process knows about. The standalone daemon and the worker log
   `Available backends:` followed by one `Backend: <name>, Qubits: <n>, Status: <n>` line per entry,
   taken from their live QDMI connection. The selector starts with an empty registry and fills it
   from the worker status messages that arrive on `[selector].backendStatusQueue`.
3. the start banner — `Starting MQSS QRM&CI Daemon.`,
   `Starting MQSS QRM&CI Distributed Backend Selector.`, or
   `Starting MQSS QRM&CI Distributed Worker.`
4. a quiet poll loop. With no work on the intake queue, a healthy daemon logs nothing further; it
   wakes every `common.stagePollInterval`, refreshes its backend registry entry, and polls the queue
   again.

If the broker is unreachable, the daemon does not exit. Each turn logs a warning of the form
`Could not read from queue '<queue>': ...` and the loop retries, so the daemon connects on its own
once RabbitMQ becomes available. A steady stream of those warnings means the broker, credentials, or
queue names are wrong — not that the daemon has died.

## Verify it works

When you built the integration targets and have a live deployment running, the client-only
integration checks under `tests/integration/` can submit work to it. For the baseline submission
path, run:

```bash
ctest --test-dir build/tests --output-on-failure -R '^submit_task$'
```

The test itself only needs to reach the same RabbitMQ broker and queues your daemon is configured
with — it is a client, not a device user. The QDMI driver and device are required by the running
daemon. A successful run means the client submitted a task, the daemon selected a backend, compiled
it, and executed it, and a result rather than a cancellation came back on the result queue.

## Where to go next

- To compose the pipeline differently instead of running a shipped daemon, see
  [Build custom QRM&CI workflow](using-the-library.md).
- To change QRM&CI itself, see [Development Guide](development-guide.md).
