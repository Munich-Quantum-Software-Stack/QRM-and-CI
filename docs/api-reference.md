# API Reference

Use this page as an index into the generated Doxygen pages for QRM&CI's public interfaces. For how
these pieces fit together, see [Build custom QRM&CI workflow](using-the-library.md); this page is for
the exact types and signatures.

## Configuration and logging

- [Config](@ref mqss::qrmci::Config), [RabbitMqConnectionConfig](@ref mqss::qrmci::RabbitMqConnectionConfig),
  [SelectorConfig](@ref mqss::qrmci::SelectorConfig), [CompilerConfig](@ref mqss::qrmci::CompilerConfig), and
  [SubmitterConfig](@ref mqss::qrmci::SubmitterConfig) - the runtime configuration model
- [loadConfig](@ref mqss::qrmci::loadConfig) - the code-backed defaults < file < environment loading
  path
- [makeLogger](@ref mqss::qrmci::makeLogger) - the console-plus-file logger setup every shipped
  entrypoint uses

## Messaging

- [CommunicationHandler](@ref mqss::qrmci::CommunicationHandler) - the templated RabbitMQ
  `send()`/`receive()` helpers. Explicitly non-copyable and non-movable: construct one in place
  rather than relocating it, and only `mqss::QuantumTask`, `mqss::QuantumResult`, and `mqss::Backend`
  have a `messageLabel()` overload -- a new message type needs one before `send()`/`receive()` for it
  will compile
- [DefaultReceiveTimeout](@ref mqss::qrmci::DefaultReceiveTimeout) - the default poll timeout for
  `receive()`
- [WaitForever](@ref mqss::qrmci::WaitForever) - the timeout value that polls until a message
  arrives or shutdown is requested
- [receiveNext](@ref mqss::qrmci::receiveNext) and [drainQueue](@ref mqss::qrmci::drainQueue) - the
  one-message and drain-the-queue loops a daemon turn is built from; both take the caller's
  termination flag explicitly and stop consuming once it is set
- [sendFailure](@ref mqss::qrmci::sendFailure) and [sendResult](@ref mqss::qrmci::sendResult) -
  replying to a task's `result_destination`

## Backends

- [BackendWrapper](@ref mqss::qrmci::BackendWrapper) - one backend's capabilities and status
- [BackendRegistry](@ref mqss::qrmci::BackendRegistry) - backend liveness tracking and deterministic
  iteration
- [PublicationThrottle](@ref mqss::qrmci::PublicationThrottle) - the unrelated concern of pacing a
  device-owning process's own status refresh/publication, independently of registry liveness
- [BackendSelectionPolicy](@ref mqss::qrmci::BackendSelectionPolicy) - the backend tie-break policies

## Daemon scaffolding

- [initializeDaemon](@ref mqss::qrmci::initializeDaemon) - signal handlers, configuration, and the
  process-wide default logger, in one call
- [openConfiguredDevice](@ref mqss::qrmci::openConfiguredDevice) - open the QDMI device the
  configuration names
- [registerOwnDevice](@ref mqss::qrmci::registerOwnDevice) and
  [refreshOwnDeviceStatus](@ref mqss::qrmci::refreshOwnDeviceStatus) - seed and refresh a
  device-owning process's own registry entry
- [selectCompileAndSchedule](@ref mqss::qrmci::selectCompileAndSchedule) and
  [compileAndSchedule](@ref mqss::qrmci::compileAndSchedule) - the intake stage, with and without
  backend selection
- [TaskScheduler](@ref mqss::qrmci::TaskScheduler) - the scheduler type every submitting daemon runs
  its tasks through
- [terminationRequested](@ref mqss::qrmci::terminationRequested) and
  [installTerminationHandlers](@ref mqss::qrmci::installTerminationHandlers) - the shared
  clean-shutdown contract

## Pipeline operations

- [chooseBackend](@ref mqss::qrmci::chooseBackend) - the selection rule, without mutating the task
- [compileQuantumTask](@ref mqss::qrmci::compileQuantumTask) - the task compilation step
- [submitQuantumTask](@ref mqss::qrmci::submitQuantumTask) - submit work without blocking for the
  result
- [collectQuantumResult](@ref mqss::qrmci::collectQuantumResult) - collect the result of a previously
  submitted job
- [executeQuantumTask](@ref mqss::qrmci::executeQuantumTask) - the submit-and-collect path in one call
- Each of the three above has an interruptible overload taking a `const std::atomic<bool>&`
  termination flag, for a daemon work loop that must shut down promptly; the flag-less overloads
  delegate to it with a flag that is never set and remain unsuitable for daemons. `collectQuantumResult`
  waits in short slices rather than one blocking call so the flag is checked between slices even
  while a job is still running.
- [buildQuantumResult](@ref mqss::qrmci::buildQuantumResult) - turn a job-result histogram into the
  result message. Requires exact cardinality: the histogram must have one entry per the task's
  circuit files, every entry must carry counts, and every count must fit in the wire format's
  `int32_t` -- a missing entry, a cardinality mismatch, or an out-of-range count is a `DeviceError`
  naming the circuit index (or the offending bitstring and value) rather than a silently trimmed
  result
- [cancelQuantumTask](@ref mqss::qrmci::cancelQuantumTask) - the standard cancellation-result shape
- Every value/error-returning function above is `[[nodiscard]]`: a caller that means to ignore a
  result must say so explicitly (e.g. `std::ignore =`) rather than discard it by accident

## Errors

- [Error](@ref mqss::qrmci::Error) - the shared failure type for public QRM&CI operations
- [Error::Kind](@ref mqss::qrmci::Error::Kind) - the categories of failure a QRM&CI operation can
  report, including `ShutdownRequested` - returned by an interruptible pipeline operation abandoned
  because shutdown was requested, not a fault of the task or the device
- [toString](@ref mqss::qrmci::toString) - a stable name for an error kind in logs

## Scope

Two headers are deliberately absent. `ConstantsMapping.h` holds the library's own translations
between the submitter, mqss protocol, and mqss-ci enumerations. `CircuitFormatPolicy.h` holds the
task/compiler/backend circuit-format admission policy `BackendWrapper::canRun()` and
`compileQuantumTask()` are built from. Treat both as implementation detail; they may change without
notice.

If you are changing these interfaces rather than calling them, see
[Development Guide](development-guide.md).
