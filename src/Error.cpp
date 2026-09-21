/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Error.h"

#include <string_view>

namespace mqss::qrmci {

std::string_view toString(Error::Kind kind) noexcept {
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
  case Error::Kind::DriverUnavailable:
    return "DriverUnavailable";
  case Error::Kind::ShutdownRequested:
    return "ShutdownRequested";
  }
  return "Unknown";
}

} // namespace mqss::qrmci
