/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file Error.h
/// @brief The error type every std::expected in QRM&CI's public interfaces
///        carries. It exists so that a caller can tell "retry this later"
///        from "reject this permanently" without matching on error prose.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mqss::qrmci {

/// @brief A failure reported by a QRM&CI operation: a kind the caller can
///        act on, plus a human-readable detail for logs and result messages.
///
/// Categorically different failures -- a backend that is merely not up yet, a
/// circuit format that will never be accepted, a violated invariant -- need
/// different responses, and the wording of a message is no basis for telling
/// them apart. The kind states the category outright; the detail carries the
/// specifics for the log line and the cancellation message.
struct Error {
  /// @brief What kind of failure occurred. This, not the wording of
  ///        @ref detail, is what callers dispatch on.
  enum class Kind : std::uint8_t {
    /// No registered backend can run the task.
    NoBackendAvailable,
    /// The circuit format is not one the compiler or the backend accepts --
    /// the submitter's input is at fault.
    UnsupportedFormat,
    /// The compiler rejected the task's circuit -- the submitter's input is
    /// at fault.
    CompilationFailed,
    /// The device refused the job at submission time.
    SubmissionFailed,
    /// The device took the job but produced no usable result.
    DeviceError,
    /// A messaging operation against the broker failed.
    MessagingFailed,
    /// An invariant this process relies on was violated, for a human to see.
    Internal,
    /// The configuration file exists but could not be parsed or failed
    /// validation. A human has to fix the file.
    ConfigError,
    /// The QDMI driver could not be used: the driver library was not found or
    /// would not load, the configured device does not exist, or the driver
    /// speaks a QDMI version this build cannot -- every one of these is a
    /// deployment fault.
    DriverUnavailable,
    /// Daemon shutdown was requested while an interruptible operation was in
    /// progress. Not a fault of the task or the device -- the caller should
    /// not send a result for it and should let its work loop exit instead.
    ShutdownRequested,
  };

  /// @brief The kind of failure, for a caller deciding what to do next.
  Kind kind;
  /// @brief A human-readable description, for log lines and for the
  ///        cancellation message sent back to the task's submitter.
  std::string detail;
};

/// @brief Name an error kind for a log line.
/// @param kind The kind to name.
/// @return The enumerator's own name, e.g. `NoBackendAvailable`.
[[nodiscard]] std::string_view toString(Error::Kind kind) noexcept;

} // namespace mqss::qrmci
