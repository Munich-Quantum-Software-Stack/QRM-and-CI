# Build custom QRM&CI workflow

This guide is for building your own workflow against the `qrmci` library. Use it when the shipped
daemons are close to what you need but split the pipeline differently than you want, or when you
need a different compiler, scheduler, or submitter behind the same stages.

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
| [BackendWrapper](@ref mqss::qrmci::BackendWrapper)                                                                                                                                                                                                                                                                                                               | you need a backend capability snapshot, from either a live device or a published backend-status message           |
| `mqss::submitter::Client`, `mqss::submitter::Device` and `mqss::submitter::Job`                                                                                                                                                                                                                                                                                  | you need to talk to a quantum device without taking on a QDMI dependency of your own                              |
| [BackendRegistry](@ref mqss::qrmci::BackendRegistry) and [BackendSelectionPolicy](@ref mqss::qrmci::BackendSelectionPolicy)                                                                                                                                                                                                                                      | you need deterministic backend selection plus backend liveness tracking                                           |
| [PublicationThrottle](@ref mqss::qrmci::PublicationThrottle)                                                                                                                                                                                                                                                                                                     | you own a device and want to pace its status refresh/publication independently of registry liveness               |
| [chooseBackend](@ref mqss::qrmci::chooseBackend), [compileQuantumTask](@ref mqss::qrmci::compileQuantumTask), [submitQuantumTask](@ref mqss::qrmci::submitQuantumTask), [collectQuantumResult](@ref mqss::qrmci::collectQuantumResult), [executeQuantumTask](@ref mqss::qrmci::executeQuantumTask), and [cancelQuantumTask](@ref mqss::qrmci::cancelQuantumTask) | you are assembling the pipeline stages themselves                                                                 |
| `Daemon.h` — [initializeDaemon](@ref mqss::qrmci::initializeDaemon), [openConfiguredDevice](@ref mqss::qrmci::openConfiguredDevice), [registerOwnDevice](@ref mqss::qrmci::registerOwnDevice), [refreshOwnDeviceStatus](@ref mqss::qrmci::refreshOwnDeviceStatus)                                                                                                | you want the shipped daemons' bring-up and backend-liveness handling rather than your own                         |
| `DaemonMessaging.h` — [receiveNext](@ref mqss::qrmci::receiveNext), [drainQueue](@ref mqss::qrmci::drainQueue), [sendFailure](@ref mqss::qrmci::sendFailure), [sendResult](@ref mqss::qrmci::sendResult)                                                                                                                                                         | you are writing a work loop and want its queue I/O to behave like the shipped ones                                |
| [Error](@ref mqss::qrmci::Error), [Error::Kind](@ref mqss::qrmci::Error::Kind), and [toString](@ref mqss::qrmci::toString)                                                                                                                                                                                                                                       | you need to dispatch on the kind of failure a QRM&CI operation reported                                           |

### Ownership and lifetime rules

- `loadConfig()` returns a `Config` value. The caller owns it and passes the relevant pieces to the
  components it constructs. `initializeDaemon()` wraps it together with the signal handlers and the
  default logger, which is what the shipped entrypoints call.
- `CommunicationHandler` is constructed from `config.common.connection` and owns the messaging object
  behind its `send()` and `receive()` helpers. It is explicitly non-copyable and non-movable, so
  construct it in place where it will live rather than relocating it later.
- `BackendWrapper` is a snapshot of backend state, built from either a live device
  (`fromSubmitter()`) or a published `mqss::Backend` message, then queried through getters and
  capability checks. It has no setters: refreshing a backend means building a new snapshot.
- `BackendRegistry` owns the per-process view of backend liveness, and only that: a `PublicationThrottle`
  is the separate, unrelated clock that paces a device-owning process's own status refresh/publication.
  A process that owns a device constructs both; the distributed selector, which never publishes,
  constructs only a registry.
- Only a process that owns a `mqss::submitter::Device` also owns a live QDMI connection. In the
  shipped entrypoints that means the standalone daemon and the distributed worker. The distributed
  selector never opens a device.

### The device boundary: `Client`, `Device` and `Job`

`Client`, `Device` and `Job` (in MQSS-Submitter's `include/mqss/submitter/`) are the only interface
through which QRM&CI reaches a quantum device, and they are deliberately QDMI-free: no QDMI type,
enum, handle or loader detail appears in them. `Device` reuses the protocol's own
`mqss::BackendStatus` and `mqss::CircuitFormat` rather than carrying parallel enums of its own, so a
backend status or circuit format read from a device needs no translation before it goes into a
`BackendWrapper` or onto the wire.

Every fallible operation reports failure through `std::expected<..., mqss::submitter::Error>` rather
than throwing. `Error` carries a generic `ErrorCode` (`Fatal`, `OutOfMem`, `NotImplemented`,
`LibNotFound`, `NotFound`, `OutOfRange`, `InvalidArgument`, `PermissionDenied`, `NotSupported`,
`BadState`, `Timeout`, `WarnGeneral`) plus a `message()`. Because that code cannot tell a refused
submission from a device fault, QRM&CI names the `Error::Kind` at the call site and formats the
detail with `toQrmciError(kind, error)`.

`Client::openDevice()` loads a driver and opens one of its devices in a single call. QRM&CI wraps it
as [openConfiguredDevice](@ref mqss::qrmci::openConfiguredDevice), which fills the two option structs
from a `SubmitterConfig` and reports failure as `Error::Kind::DriverUnavailable`; the call underneath
looks like this:

```cpp
mqss::submitter::Client client;
auto openedDevice = client.openDevice(
    std::filesystem::path{config.submitter.qdmiDriver},
    config.submitter.qdmiDeviceName,
    mqss::submitter::SessionConfig{
        .token = config.submitter.qdmiClientToken.empty()
                     ? std::nullopt
                     : std::optional{config.submitter.qdmiClientToken}},
    mqss::submitter::DeviceConfig{
        .deviceId = config.submitter.qdmiDeviceId.empty()
                        ? std::nullopt
                        : std::optional{config.submitter.qdmiDeviceId}});
if (!openedDevice) {
  spdlog::error("Could not open the QDMI device: {}",
                openedDevice.error().message());
  return EXIT_FAILURE;
}
const mqss::submitter::Device &device = *openedDevice;
```

Check the `expected` before using it. The QDMI version is detected from the driver rather than
declared by the deployment, so a version mismatch is one of the failures `openDevice()` returns
rather than something the configuration has to get right. `client` need not outlive the `Device`:
`Device` holds a `shared_ptr` to the driver's control block internally.

`qdmiDriver` is a **filesystem path**, not a library stem: `openDevice()` checks that the file
exists before loading it, and does not search the library path for it.

A `Job` is a move-only RAII handle to one submitted circuit. `Device` and `Job` have private
constructors reachable only through `Client`/`Driver`/`Device`, so there is no interface left to
implement with a fake — the submit/collect failure paths are covered only by what a real driver
produces.

### Reading a backend: `BackendWrapper::fromSubmitter()`

`BackendWrapper::fromSubmitter()` returns `std::expected<BackendWrapper, Error>` rather than being a
constructor, because reading a backend means five separate device queries and any of them can fail —
which a constructor has nowhere to report. It queries in a fixed order and returns the first failure,
so a device that fails midway yields an error rather than a half-populated wrapper that still looks
usable.

`fromSubmitter()` also takes an optional `dispatchQueue` parameter (default empty), which becomes the
resulting wrapper's `queueName`. A process that never publishes its status (the standalone daemon,
which selects and dispatches to itself) can leave it empty; a distributed worker passes its own
`config.compiler.queue` so the queue name travels with every published status and the selector can
route a chosen task back to the exact worker that advertised it.

### Trusting a published backend: `BackendWrapper::fromPublishedStatus()`

A backend-status message received over the wire did not come from this process reading its own
device, so it gets a different entry point: `BackendWrapper::fromPublishedStatus()` returns
`std::expected<BackendWrapper, Error>`, rejecting a message with an empty `name` or `queue_name`
rather than registering an entry nothing can be dispatched to. Its raw constructor stays private for
this reason -- production code that needs a `BackendWrapper` from an `mqss::Backend` message always
goes through validation. Unknown wire values for `type`, `status`, and circuit formats each normalize
to their own `*_UNSPECIFIED` enumerator instead of propagating an out-of-range value, so a peer built
against a newer protocol version degrades gracefully instead of corrupting comparisons downstream.

A task's `restricted_resource_names` is an allow-list read by `BackendWrapper::canRun()`: empty means
unrestricted, and a non-empty list means the task can run only on a backend whose name appears in it
-- checked before circuit-format admission, and enforced even against the task's own
`preferred_qpu()`.

### Error handling

The orchestration helpers in `include/qrmci/` use `std::expected<..., Error>` rather than
exceptions. The contract is:

- match on `Error::Kind` when you decide how to log or report a failure
- keep `error.detail` for logs and returned cancellation reasons
- `Error::Kind::DriverUnavailable` covers a driver library that would not load, a configured device
  that does not exist, and a driver speaking a QDMI version this build cannot — every one of them a
  deployment fault.
- `Error::Kind::ShutdownRequested` is not a fault of the task or the device: it is what the
  interruptible `submitQuantumTask`/`collectQuantumResult`/`executeQuantumTask` overloads return when
  the caller's termination flag was set before they could finish. Do not `sendFailure(...)` for it —
  let the work loop exit instead, since the caller is shutting down anyway.
- use `toString(Error::Kind)` when you need a stable log label for the kind itself

Do not parse human-readable error strings to recover behavior the enum already carries.

## Reference compositions

### The standalone daemon: the entire pipeline in one process

`apps/standalone/main.cpp` is the reference composition in this repository. Its order is:

1. Call `initializeDaemon()`, which installs the `SIGINT`/`SIGTERM` handlers, assembles the
   configuration, and makes the daemon logger the process-wide default. Stop if it fails; nothing is
   logged yet at that point, so the entrypoint reports the error itself.
2. Construct `CommunicationHandler` and a `TaskScheduler`, then open the device with
   `openConfiguredDevice(config.submitter)` and stop with a nonzero exit if that fails.
3. Construct `mqss::mqssci::MQSSCompiler`, `BackendRegistry`, and a `PublicationThrottle` from
   `config.submitter.backendStatusPublishInterval`, and seed the registry with
   `registerOwnDevice(...)` — also stopping if the device cannot be read even once.
4. Log the registry with `logRegisteredBackends(...)`, then loop until `terminationRequested` is set.
5. In each loop turn:
   1. `refreshOwnDeviceStatus(...)`, which re-reads the device only when the throttle has a slot due
   2. `expire()` stale backend registry entries
   3. `drainQueue<mqss::QuantumTask>(...)` on `config.common.qrmciQueue`, passing `terminationRequested`
      explicitly and each task to `selectCompileAndSchedule(...)`
   4. on any failure there — no backend, a backend that expired between selection and lookup, or a
      compile error — `sendFailure(...)` puts a cancellation result on the task's
      `result_destination()`; the task never reaches the scheduler
   5. drain the scheduler's ready jobs with `executeQuantumTask(...)`, also passing
      `terminationRequested` so a shutdown mid-job is honoured, sending the `mqss::QuantumResult` with
      `sendResult(...)` on success and a cancellation with `sendFailure(...)` on failure — except when
      the failure's kind is `Error::Kind::ShutdownRequested`, which means shutdown was requested while
      the job was in flight; the loop logs it and exits without sending anything, since the job is
      abandoned rather than failed
   6. sleep for `config.common.stagePollInterval`, but only if the turn found neither a task nor a
      ready job

Four details matter when you reproduce this pattern:

- the refresh and the expiry come first, ahead of the receive, so a quiet intake queue cannot starve
  the refresh and let the daemon expire its own backend out from under itself
- the intake is a non-blocking drain rather than a bounded wait, so a burst of tasks is handled in
  one turn instead of one task per poll interval
- the standalone daemon submits one ready job at a time through `executeQuantumTask(...)`
- a refresh that fails is logged and skipped rather than fatal; only the _first_ read of the device,
  at startup, is treated as a reason not to run at all

### The distributed daemons: the pipeline split across two processes

`apps/distributed/selector/main.cpp` and `apps/distributed/worker/main.cpp` keep the same building
blocks but change where they are used:

- the **selector** owns `CommunicationHandler` and `BackendRegistry`. It opens no device, so it
  drains backend-status messages from `config.selector.backendStatusQueue` into its registry through
  `BackendWrapper::fromPublishedStatus(...)` -- logging and dropping any message that fails
  validation, and also logging and dropping a status whose ID is already registered under a
  different non-empty queue (`BackendRegistry::insertOrRefresh()` returning `false`), since applying
  it would mean routing future tasks to the wrong worker -- drains tasks from
  `config.common.qrmciQueue`, runs `chooseBackend(...)`, then re-resolves the chosen ID in its
  registry and forwards the task to that backend's own `getQueueName()` rather than any single
  configured queue, since each worker in a multi-worker deployment advertises its own.
- the **worker** owns `CommunicationHandler`, a `TaskScheduler`, the `mqss::submitter::Device` it
  opened, `mqss::mqssci::MQSSCompiler`, a `BackendRegistry`, and a `PublicationThrottle`. It publishes
  whatever snapshot `refreshOwnDeviceStatus(device, registry, throttle, config.compiler.queue)`
  returns to `config.selector.backendStatusQueue` -- carrying its own `config.compiler.queue` as the
  dispatch queue the selector should route back to -- takes tasks from that same
  `config.compiler.queue` through `compileAndSchedule(...)` — which compiles for the backend the
  selector already assigned rather than choosing one — and then submits and collects.

Two behavioral differences from standalone:

- the worker takes one task per turn rather than draining, so the jobs it has already scheduled get
  submitted without waiting for the intake queue to run dry first
- the worker submits every ready job before collecting any result, so several jobs can be in flight
  on the device at once

Like the standalone daemon, the worker passes `terminationRequested` explicitly to `receiveNext(...)`
and to the interruptible `submitQuantumTask(...)`/`collectQuantumResult(...)` overloads, and treats an
`Error::Kind::ShutdownRequested` result from either the same way: log it and exit the loop without
sending a failure, since the task was abandoned for shutdown rather than actually failed.

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

From the root `CMakeLists.txt`, `qrmci` already links `mqss`, `mqss-ci::mqss-ci`, `scheduler`,
`mqss_submitter`, `tomlplusplus::tomlplusplus`, and the configured spdlog target, and it exports
them, so a target that links `qrmci` gets all of them too.

## Deploying what you built

Your custom entrypoint uses the same configuration layering and runtime contract as the shipped
daemons: defaults, then an optional TOML file, then environment variables. Reuse
[Getting Started](getting-started.md#runtime-configuration) for the config files, environment
variables, and deployment checks.

## Where to look next

- For exact public signatures and symbol pages, use [API Reference](api-reference.md).
- If your change belongs upstream in QRM&CI itself, use [Development Guide](development-guide.md).
