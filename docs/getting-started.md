# Getting Started

This guide covers building, testing, and generating documentation for the Quantum Resource Manager and Compiler Infrastructure (QRM&CI).

## Prerequisites

Install the following tools and libraries on your development machine:

- CMake 3.20 or newer
- Linux (CI uses Ubuntu 24.04)
- A C++23-capable compiler: GCC 13+ or Clang 17+
- CI also tests GCC 14 and Clang 18
- Doxygen (optional, only required for API docs generation)
- Dependencies required by MQSS components, resolved through your configured environment and CMake package discovery:
  - MQSS Integration and Deployment Framework
  - Compiler
  - Scheduler
  - Submitter

### Required System Packages

CI installs these packages on Ubuntu 24.04 (see `.github/workflows/ci.yml`); use
your distribution's equivalents locally:

- `build-essential`, `cmake` (3.20+), `ninja-build`, `pkg-config`, `git`
- A supported compiler: `gcc-13`/`g++-13` or newer, or `clang-17`/`clang++-17` or newer
- `libboost-all-dev`, `libboost-graph-dev`
- `libfmt-dev`, `libgtest-dev`, `libspdlog-dev`
- `librabbitmq-dev`
- `nlohmann-json3-dev`
- `libz3-dev`, `libzstd-dev`
- `libprotobuf-dev`, `protobuf-compiler`
- `python3-dev`, `python3-pip`
- `openssh-client` (for SSH-based `git` FetchContent dependencies)

## Configure and Build

From the repository root, configure into a disposable build directory. The
directory may be removed and recreated at any time:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

This produces the standalone daemon executable at:

- build/apps/standalone/qrmcid-standalone

To build without the application, set `-DQRMCI_BUILD_APPS=OFF`. Unit and
integration test builds can likewise be disabled with
`-DQRMCI_BUILD_UNIT_TESTS=OFF` and `-DQRMCI_BUILD_INTEGRATION_TESTS=OFF`.

## Run Tests

Run the tests registered by the configured project and its MQSS dependencies
with CTest:

```bash
ctest --test-dir build --output-on-failure
```

The QRM integration test executable is `submit_task`. The configured MQSS
environment may also register transport and serialization contract tests. The
submitter and end-to-end workflow require a reachable RabbitMQ instance and a
configured QDMI driver.

For an end-to-end pipeline check, run `qrmcid-standalone` in one terminal and
the built `submit_task` executable in another. The daemon handles `SIGINT` and
`SIGTERM` for graceful shutdown.

## Runtime Configuration

The daemon reads configuration from environment variables, with defaults
compiled from `include/qrmci/ConfigDefaults.h.in`. The supported variables are:

- QRM_AMQP_HOST
- QRM_AMQP_PORT
- QRM_AMQP_USER
- QRM_AMQP_PASSWORD
- QRM_AMQP_VHOST
- QRM_QRMCI_QUEUE
- QRM_SCHEDULER_QUEUE
- QRM_COMPILER_QUEUE
- QRM_RESULTS_QUEUE
- QRM_SUBMITTER_QUEUE
- QRM_LOG_DIR
- SUBMITTER_QDMI_DRIVER_NAME
- SUBMITTER_QDMI_DEVICE_NAME
- SUBMITTER_QDMI_CLIENT_TOKEN

The default RabbitMQ host is `host.docker.internal`; the default log directory is
`/var/log/qrmci`. For the full configuration model and loading logic, see
`include/qrmci/Config.h` and `src/Config.cpp`.

## Generate API Documentation

Doxygen is optional unless API documentation is required. Configure with
`BUILD_QRMCI_DOCS=ON` and build the `docs` target:

```bash
cmake -S . -B build -DBUILD_QRMCI_DOCS=ON
cmake --build build --target docs
```

The generated site is written to `build/docs/html/index.html`.
