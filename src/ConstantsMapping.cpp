/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/ConstantsMapping.h"

#include "mqss/Protocol.hpp"
#include "qrmci/Error.h"

#include <expected>
#include <string>
#include <string_view>

namespace mqss::qrmci {

namespace {

/// @brief Name a submitter error code for a log line. Mirrors
///        toString(Error::Kind) in Error.cpp; kept here because the code is
///        the only part of a submitter::Error that does not already read as
///        prose.
std::string_view
submitterErrorCodeName(mqss::submitter::ErrorCode code) noexcept {
  switch (code) {
  case mqss::submitter::ErrorCode::Fatal:
    return "Fatal";
  case mqss::submitter::ErrorCode::OutOfMem:
    return "OutOfMem";
  case mqss::submitter::ErrorCode::NotImplemented:
    return "NotImplemented";
  case mqss::submitter::ErrorCode::LibNotFound:
    return "LibNotFound";
  case mqss::submitter::ErrorCode::NotFound:
    return "NotFound";
  case mqss::submitter::ErrorCode::OutOfRange:
    return "OutOfRange";
  case mqss::submitter::ErrorCode::InvalidArgument:
    return "InvalidArgument";
  case mqss::submitter::ErrorCode::PermissionDenied:
    return "PermissionDenied";
  case mqss::submitter::ErrorCode::NotSupported:
    return "NotSupported";
  case mqss::submitter::ErrorCode::BadState:
    return "BadState";
  case mqss::submitter::ErrorCode::Timeout:
    return "Timeout";
  case mqss::submitter::ErrorCode::WarnGeneral:
    return "WarnGeneral";
  }
  // No default: above, so -Wswitch flags an ErrorCode added later instead of
  // silently naming it Unknown. Only reachable through a value outside the
  // enumeration.
  return "Unknown";
}

} // namespace

Error toQrmciError(Error::Kind kind, const mqss::submitter::Error &error) {
  return Error{kind, std::string(submitterErrorCodeName(error.code())) + ": " +
                         error.message()};
}

mqss::CircuitFormat mapProtoCircuitFormat(int format) noexcept {
  // protobuf keeps an enum value it does not recognise as its raw integer
  // rather than rejecting the message, so a straight static_cast here would
  // manufacture an out-of-range mqss::CircuitFormat and feed it into the
  // compatibility comparisons.
  if (!mqss::protocol::v1::CircuitFormat_IsValid(format)) {
    return mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED;
  }
  return static_cast<mqss::CircuitFormat>(format);
}

mqss::BackendType mapProtoBackendType(int type) noexcept {
  if (!mqss::protocol::v1::BackendType_IsValid(type)) {
    return mqss::BackendType::BACKEND_TYPE_UNSPECIFIED;
  }
  return static_cast<mqss::BackendType>(type);
}

mqss::BackendStatus mapProtoBackendStatus(int status) noexcept {
  if (!mqss::protocol::v1::BackendStatus_IsValid(status)) {
    return mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED;
  }
  return static_cast<mqss::BackendStatus>(status);
}

std::expected<mqss::mqssci::OptLevel, Error>
getCompilerOptimizationLevel(int level) {
  switch (level) {
  case 0:
  case 1:
    return mqss::mqssci::OptLevel::O1;
  case 2:
    return mqss::mqssci::OptLevel::O2;
  case 3:
    return mqss::mqssci::OptLevel::O3;
  default:
    return std::unexpected(
        Error{Error::Kind::CompilationFailed, "Invalid optimisation level " +
                                                  std::to_string(level) +
                                                  "; expected 0-3."});
  }
}

bool isOnlineBackendStatus(mqss::BackendStatus status) noexcept {
  return status == mqss::BackendStatus::BACKEND_STATUS_IDLE ||
         status == mqss::BackendStatus::BACKEND_STATUS_BUSY;
}

std::string circuitFormatLabel(mqss::CircuitFormat format) {
  if (mqss::protocol::v1::CircuitFormat_IsValid(format)) {
    return mqss::protocol::v1::CircuitFormat_Name(format);
  }
  return "UNKNOWN(" + std::to_string(static_cast<int>(format)) + ")";
}

std::string backendStatusLabel(mqss::BackendStatus status) {
  if (mqss::protocol::v1::BackendStatus_IsValid(status)) {
    return mqss::protocol::v1::BackendStatus_Name(status);
  }
  return "UNKNOWN(" + std::to_string(static_cast<int>(status)) + ")";
}

} // namespace mqss::qrmci
