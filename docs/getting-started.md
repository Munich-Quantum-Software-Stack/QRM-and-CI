# Getting Started

This guide helps you build, test, and generate documentation for the Quantum Resource Manager and Compiler Infrastructure (QRM&CI).

## Prerequisites

Install the following tools and libraries on your development machine:

- CMake 3.20 or newer
- A C++23-capable compiler (for example GCC 13+ or Clang 17+)
- Doxygen (optional, only required for API docs generation)
- Dependencies required by MQSS components (resolved via your configured environment and CMake package discovery)
  - MQSS Integration and Deployment Framework
  - Compiler
  - Backend Selector
  - Scheduler
  - Submitter

## Configure and Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

This produces the daemon executable at:

- build/deplyoment/workflow-aio/qrmci_aio_daemon

## Run Unit/Integration Tests

For end to end pipeline testing

- Run `qrmci_aio_daemon` in one terminal
- Run `submit_task` in another terminal

<!--
After a successful build, run tests with CTest:

```bash
ctest --test-dir build --output-on-failure
```
The current test target includes `submit_task` and is registered through the CMake test suite.
-->

## Runtime Configuration

The daemon reads configuration from environment variables, with defaults compiled from ConfigDefaults.
Common variables include:

- QRM_AMQP_HOST
- QRM_AMQP_PORT
- QRM_AMQP_USER
- QRM_AMQP_PASSWORD
- QRM_QRMCI_QUEUE
- SUBMITTER_QDMI_DRIVER_NAME
- QDMI_CONF

For full mapping, inspect configuration loading logic in `src/Config.cpp` and declarations in `include/Config.hpp`.
