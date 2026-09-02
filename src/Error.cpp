/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Error.h"

#include <string_view>

bool mqss::qrmci::Error::isRetryable() const noexcept {
  switch (kind) {
  case Kind::NoBackendAvailable:
  case Kind::SubmissionFailed:
  case Kind::DeviceError:
  case Kind::MessagingFailed:
    return true;
  case Kind::UnsupportedFormat:
  case Kind::CompilationFailed:
  case Kind::Internal:
  case Kind::ConfigError:
    return false;
  }
  // Only reachable through a value outside the enumeration; treating it as
  // permanent keeps a corrupted kind from being requeued forever.
  return false;
}

std::string_view mqss::qrmci::toString(Error::Kind kind) noexcept {
  switch (kind) {
  case Error::Kind::NoBackendAvailable:
    return "NoBackendAvailable";
  case Error::Kind::UnsupportedFormat:
    return "UnsupportedFormat";
  case Error::Kind::CompilationFailed:
    return "CompilationFailed";
  case Error::Kind::SubmissionFailed:
    return "SubmissionFailed";
  case Error::Kind::DeviceError:
    return "DeviceError";
  case Error::Kind::MessagingFailed:
    return "MessagingFailed";
  case Error::Kind::Internal:
    return "Internal";
  case Error::Kind::ConfigError:
    return "ConfigError";
  }
  return "Unknown";
}
