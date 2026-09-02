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
  `send()`/`receive()` helpers
- [DefaultReceiveTimeout](@ref mqss::qrmci::DefaultReceiveTimeout) - the default poll timeout for
  `receive()`

## Backends

- [BackendWrapper](@ref mqss::qrmci::BackendWrapper) - one backend's capabilities and status
- [BackendRegistry](@ref mqss::qrmci::BackendRegistry) - backend liveness tracking, deterministic
  iteration, and status-publication pacing
- [BackendSelectionPolicy](@ref mqss::qrmci::BackendSelectionPolicy) - the backend tie-break policies

## Pipeline operations

- [chooseBackend](@ref mqss::qrmci::chooseBackend) - the selection rule, without mutating the task
- [compileQuantumTask](@ref mqss::qrmci::compileQuantumTask) - the task compilation step
- [submitQuantumTask](@ref mqss::qrmci::submitQuantumTask) - submit work without blocking for the
  result
- [collectQuantumResult](@ref mqss::qrmci::collectQuantumResult) - collect the result of a previously
  submitted job
- [executeQuantumTask](@ref mqss::qrmci::executeQuantumTask) - the submit-and-collect path in one call
- [cancelQuantumTask](@ref mqss::qrmci::cancelQuantumTask) - the standard cancellation-result shape

## Errors

- [Error](@ref mqss::qrmci::Error) - the shared failure type for public QRM&CI operations
- [Error::Kind](@ref mqss::qrmci::Error::Kind) - the retryable-versus-permanent categories
- [Error::isRetryable](@ref mqss::qrmci::Error::isRetryable) - the built-in retry decision
- [toString](@ref mqss::qrmci::toString) - a stable name for an error kind in logs

## Scope

Headers not listed here — currently `ConstantsMapping.h` — are internal support code for the
library's own translations between QDMI, mqss protocol, and mqss-ci enumerations. Treat them as
implementation detail; they may change without notice.

If you are changing these interfaces rather than calling them, see
[Development Guide](development-guide.md).
