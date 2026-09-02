# Build custom QRM&CI workflow

This guide is for building your own workflow against the `qrmci` library. Use it when the shipped
daemons are close to what you need but compose the pipeline differently than you want: a different
workflow, compiler, scheduler, or submitter.

Build your own workflow when you need to:

- split the pipeline differently from the shipped standalone or selector/worker entrypoints
- add or replace internal components while keeping the same configuration, messaging, and runner
  helpers
- embed QRM&CI into a larger process — which today means building that process inside this tree (or
  vendoring the repository), because `qrmci` is not installed or exported as a consumable CMake
  package; see "Linking against `qrmci`" below

## The building blocks

| Building block                                                                                                                                                                                                                                                                                                                                                   | Use it when...                                                                                                    |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| [Config](@ref mqss::qrmci::Config) and [loadConfig](@ref mqss::qrmci::loadConfig)                                                                                                                                                                                                                                                                                | you want the same defaults < file < environment layering as the shipped daemons, without a process-wide singleton |
| [makeLogger](@ref mqss::qrmci::makeLogger)                                                                                                                                                                                                                                                                                                                       | you want the same console-plus-file logger pattern the shipped entrypoints use                                    |
| [CommunicationHandler](@ref mqss::qrmci::CommunicationHandler)                                                                                                                                                                                                                                                                                                   | you need RabbitMQ send/receive helpers that report failure through `std::expected`                                |
| [BackendWrapper](@ref mqss::qrmci::BackendWrapper)                                                                                                                                                                                                                                                                                                               | you need a backend capability snapshot, either from a live submitter or a published backend-status message        |
| [BackendRegistry](@ref mqss::qrmci::BackendRegistry) and [BackendSelectionPolicy](@ref mqss::qrmci::BackendSelectionPolicy)                                                                                                                                                                                                                                      | you need deterministic backend selection plus backend liveness tracking                                           |
| [chooseBackend](@ref mqss::qrmci::chooseBackend), [compileQuantumTask](@ref mqss::qrmci::compileQuantumTask), [submitQuantumTask](@ref mqss::qrmci::submitQuantumTask), [collectQuantumResult](@ref mqss::qrmci::collectQuantumResult), [executeQuantumTask](@ref mqss::qrmci::executeQuantumTask), and [cancelQuantumTask](@ref mqss::qrmci::cancelQuantumTask) | you are assembling the pipeline stages themselves                                                                 |
| [Error](@ref mqss::qrmci::Error), [Error::Kind](@ref mqss::qrmci::Error::Kind), [Error::isRetryable](@ref mqss::qrmci::Error::isRetryable), and [toString](@ref mqss::qrmci::toString)                                                                                                                                                                           | you need to distinguish retryable failures from permanent ones                                                    |

### Ownership and lifetime rules

- `loadConfig()` returns a `Config` value. The caller owns it and passes the relevant pieces to the
  components it constructs.
- `CommunicationHandler` is constructed from `config.common.connection` and owns the messaging object
  behind its `send()` and `receive()` helpers.
- `BackendWrapper` is a snapshot of backend state. It is built from a live submitter or a published
  `mqss::Backend` message and then queried through getters and capability checks; the public type has
  no setters.
- `BackendRegistry` owns the per-process view of backend liveness. Its time-to-live and publish
  interval are separate clocks.
- Only a process that owns a `mqss::submitter::Submitter` also owns a live QDMI connection. In the
  shipped entrypoints that means the standalone daemon and the distributed worker. The distributed
  selector never opens a device.

### Error handling

The orchestration helpers in `include/qrmci/` use `std::expected<..., Error>` rather than
exceptions. The contract is:

- match on `Error::Kind` when you decide whether to retry, reject, or log
- use `error.isRetryable()` for the transient-versus-permanent split already encoded in the type
- keep `error.detail` for logs and returned cancellation reasons
- use `toString(Error::Kind)` when you need a stable log label for the kind itself

Do not parse human-readable error strings to recover behavior the enum already carries.

## Reference compositions

### The standalone daemon: the entire pipeline in one process

`apps/standalone/main.cpp` is the reference composition in this repository. Its order is:

1. Register `SIGINT` and `SIGTERM` handlers.
2. Call `loadConfig()` and stop immediately if configuration loading fails.
3. Create the default logger with `makeLogger(...)`.
4. Construct `CommunicationHandler`, `mqss::Scheduler<mqss::QuantumTask>`,
   `mqss::submitter::Submitter`, `mqss::mqssci::MQSSCompiler`, and `BackendRegistry`.
5. Seed the registry with `BackendWrapper(submitter)`.
6. In each loop turn:
   1. refresh the process's own backend when `claimPublishSlot()` says it is due
   2. expire stale backend registry entries
   3. receive the next `mqss::QuantumTask` from `config.common.qrmciQueue`
   4. choose a backend with `chooseBackend(...)`
   5. write the chosen backend name into `task.scheduled_qpu()`
   6. compile the task with `compileQuantumTask(...)`
   7. on any failure up to this point — no backend, a backend that expired between selection and
      lookup, or a compile error — send `cancelQuantumTask(task, failure.detail)` to the task's
      `result_destination()`; the task never reaches the scheduler
   8. otherwise hand the compiled task to the scheduler
   9. drain the ready jobs with `executeQuantumTask(...)`, sending the `mqss::QuantumResult` on
      success and a `cancelQuantumTask(...)` result on failure, both to the job's
      `result_destination()`
   10. sleep for `config.common.stagePollInterval` before the next turn

Two details matter when you reproduce this pattern:

- the receive on `config.common.qrmciQueue` uses `config.selector.taskReceiveTimeout`, not an
  unbounded wait, so the loop keeps getting turns to refresh its own backend registry entry
- the standalone daemon submits one ready job at a time through `executeQuantumTask(...)`

### The distributed daemons: the pipeline split across two processes

`apps/distributed/selector/main.cpp` and `apps/distributed/worker/main.cpp` keep the same building
blocks but change where they are used:

- the **selector** owns `CommunicationHandler` and `BackendRegistry`, receives backend-status
  messages on `config.selector.backendStatusQueue`, receives tasks on `config.common.qrmciQueue`,
  runs `chooseBackend(...)`, and forwards selected tasks to `config.compiler.queue`
- the **worker** owns `CommunicationHandler`, `mqss::Scheduler<mqss::QuantumTask>`,
  `mqss::submitter::Submitter`, `mqss::mqssci::MQSSCompiler`, and `BackendRegistry`, compiles tasks
  from `config.compiler.queue`, publishes its own backend status to
  `config.selector.backendStatusQueue`, and uses `submitQuantumTask(...)` followed by
  `collectQuantumResult(...)`

The important behavioral difference is that the worker submits every ready job before collecting any
result, so several jobs can be in flight on the device at once.

## Linking against `qrmci`

The shipped applications show the supported in-tree pattern: add an executable target in this build
and link it against `qrmci`. This repository does not document or install a separate public CMake
package for consumers outside the tree.

Mirror the app targets in `apps/standalone/CMakeLists.txt` or `apps/distributed/CMakeLists.txt`:

```cmake
find_package(Threads REQUIRED)

add_executable(my-qrm main.cpp)
target_link_libraries(my-qrm PUBLIC qrmci Threads::Threads ${QRMCI_SPDLOG_TARGET})
target_include_directories(
  my-qrm PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/generated")
```

From the root `CMakeLists.txt`, `qrmci` already links `mqss`, `mqss-ci::mqss-ci`,
`mqss_scheduler`, `mqss_submitter`, `tomlplusplus::tomlplusplus`, and the configured spdlog target.

## Deploying what you built

Your custom entrypoint uses the same configuration layering and runtime contract as the shipped
daemons: defaults, then an optional TOML file, then environment variables. Reuse
[Getting Started](getting-started.md#runtime-configuration) for the config files, environment
variables, and deployment checks.

## Where to look next

- For exact public signatures and symbol pages, use [API Reference](api-reference.md).
- If your change belongs upstream in QRM&CI itself, use [Development Guide](development-guide.md).
