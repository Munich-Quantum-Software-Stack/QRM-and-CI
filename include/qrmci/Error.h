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
/// Failures used to travel as a bare std::string, which put every
/// categorically different failure -- a backend that is merely not up yet, a
/// circuit format that will never be accepted, a violated invariant -- into
/// the same shape, and left a caller wanting to requeue rather than reject
/// with nothing to match on but the wording. The kind states that outright;
/// the detail stays for the log line and the cancellation message.
struct Error {
  /// @brief What kind of failure occurred. This, not the wording of
  ///        @ref detail, is what callers dispatch on.
  enum class Kind : std::uint8_t {
    /// No registered backend can run the task. Transient: a backend may come
    /// online, or an existing one may free up.
    NoBackendAvailable,
    /// The circuit format is not one the compiler or the backend accepts.
    /// Permanent, and the submitter's input is at fault.
    UnsupportedFormat,
    /// The compiler rejected the task's circuit. Permanent, user input.
    CompilationFailed,
    /// The device refused the job at submission time. Transient.
    SubmissionFailed,
    /// The device took the job but produced no usable result. Transient.
    DeviceError,
    /// A messaging operation against the broker failed. Transient.
    MessagingFailed,
    /// An invariant this process relies on was violated. Not retryable:
    /// retrying re-runs the same broken path, so this is for a human to see.
    Internal,
    /// The configuration file exists but could not be parsed or failed
    /// validation. Not retryable: a human has to fix the file.
    ConfigError,
  };

  /// @brief The kind of failure, for a caller deciding what to do next.
  Kind kind;
  /// @brief A human-readable description, for log lines and for the
  ///        cancellation message sent back to the task's submitter.
  std::string detail;

  /// @brief Whether retrying the failed operation unchanged could succeed.
  /// @return True for the transient kinds (NoBackendAvailable,
  ///         SubmissionFailed, DeviceError and MessagingFailed), false for
  ///         the permanent ones and for Internal.
  [[nodiscard]] bool isRetryable() const noexcept;
};

/// @brief Name an error kind for a log line.
/// @param kind The kind to name.
/// @return The enumerator's own name, e.g. `NoBackendAvailable`.
[[nodiscard]] std::string_view toString(Error::Kind kind) noexcept;

} // namespace mqss::qrmci
