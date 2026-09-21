/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/Protocol.hpp"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <gtest/gtest.h>
#include <mqss/submitter/Error.h>
#include <string>

namespace mqss::qrmci::test {

// QDMI translation is not covered here: it belongs to MQSS-Submitter, behind
// its Client/Device/Job interface, and is tested enumerator-by-enumerator in
// that repository. QRM&CI's live-driver tests would only ever reach the
// handful of statuses and formats the example device happens to report.

// ===========================================================================
// MapProtoCircuitFormatTest
// ===========================================================================
TEST(MapProtoCircuitFormatTest, KnownValueIsPreserved) {
  EXPECT_EQ(mapProtoCircuitFormat(mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3);
}

TEST(MapProtoCircuitFormatTest, UnspecifiedIsPreserved) {
  EXPECT_EQ(mapProtoCircuitFormat(0),
            mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED);
}

TEST(MapProtoCircuitFormatTest, EveryKnownEnumeratorRoundTrips) {
  for (int value = mqss::protocol::v1::CircuitFormat_MIN;
       value <= mqss::protocol::v1::CircuitFormat_MAX; ++value) {
    EXPECT_EQ(mapProtoCircuitFormat(value),
              static_cast<mqss::CircuitFormat>(value));
  }
}

TEST(MapProtoCircuitFormatTest, OutOfRangeValueFallsBackToUnspecified) {
  // protobuf preserves an unknown enum value as its raw integer, so a peer
  // built against a newer protocol can put one on the wire.
  EXPECT_EQ(mapProtoCircuitFormat(mqss::protocol::v1::CircuitFormat_MAX + 1),
            mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED);
  EXPECT_EQ(mapProtoCircuitFormat(9999),
            mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED);
}

TEST(MapProtoCircuitFormatTest, NegativeValueFallsBackToUnspecified) {
  EXPECT_EQ(mapProtoCircuitFormat(-1),
            mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED);
}

// ===========================================================================
// CircuitFormatLabelTest
// ===========================================================================
TEST(CircuitFormatLabelTest, EveryKnownEnumeratorHasANonEmptyLabel) {
  for (int value = mqss::protocol::v1::CircuitFormat_MIN;
       value <= mqss::protocol::v1::CircuitFormat_MAX; ++value) {
    const auto label =
        circuitFormatLabel(static_cast<mqss::CircuitFormat>(value));
    EXPECT_FALSE(label.empty()) << "value: " << value;
    // A stable protocol name, not a bare integer.
    EXPECT_EQ(label.find_first_not_of("0123456789"), 0U) << "value: " << value;
  }
}

TEST(CircuitFormatLabelTest, KnownEnumeratorNamesTheEnumerator) {
  EXPECT_EQ(circuitFormatLabel(mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2),
            "CIRCUIT_FORMAT_QASM2");
}

TEST(CircuitFormatLabelTest, OutOfRangeValueFallsBackToUnknownWithTheNumber) {
  const auto label = circuitFormatLabel(static_cast<mqss::CircuitFormat>(
      mqss::protocol::v1::CircuitFormat_MAX + 1));
  EXPECT_EQ(label,
            "UNKNOWN(" +
                std::to_string(mqss::protocol::v1::CircuitFormat_MAX + 1) +
                ")");
}

// ===========================================================================
// BackendStatusLabelTest
// ===========================================================================
TEST(BackendStatusLabelTest, EveryKnownEnumeratorHasANonEmptyLabel) {
  for (int value = mqss::protocol::v1::BackendStatus_MIN;
       value <= mqss::protocol::v1::BackendStatus_MAX; ++value) {
    const auto label =
        backendStatusLabel(static_cast<mqss::BackendStatus>(value));
    EXPECT_FALSE(label.empty()) << "value: " << value;
    EXPECT_EQ(label.find_first_not_of("0123456789"), 0U) << "value: " << value;
  }
}

TEST(BackendStatusLabelTest, KnownEnumeratorNamesTheEnumerator) {
  EXPECT_EQ(backendStatusLabel(mqss::BackendStatus::BACKEND_STATUS_IDLE),
            "BACKEND_STATUS_IDLE");
}

TEST(BackendStatusLabelTest, OutOfRangeValueFallsBackToUnknownWithTheNumber) {
  const auto label = backendStatusLabel(static_cast<mqss::BackendStatus>(
      mqss::protocol::v1::BackendStatus_MAX + 1));
  EXPECT_EQ(label,
            "UNKNOWN(" +
                std::to_string(mqss::protocol::v1::BackendStatus_MAX + 1) +
                ")");
}

// ===========================================================================
// GetCompilerOptimizationLevelTest
// ===========================================================================
TEST(GetCompilerOptimizationLevelTest, MappedLevels) {
  // 0 and proto3's implicit-absent both mean 0 on the wire, and must select
  // O1, not the most aggressive level: an omitted optimisation_level is not
  // a request to maximize optimisation.
  auto zero = getCompilerOptimizationLevel(0);
  ASSERT_TRUE(zero.has_value()) << zero.error().detail;
  EXPECT_EQ(*zero, mqss::mqssci::OptLevel::O1);

  auto one = getCompilerOptimizationLevel(1);
  ASSERT_TRUE(one.has_value()) << one.error().detail;
  EXPECT_EQ(*one, mqss::mqssci::OptLevel::O1);

  auto two = getCompilerOptimizationLevel(2);
  ASSERT_TRUE(two.has_value()) << two.error().detail;
  EXPECT_EQ(*two, mqss::mqssci::OptLevel::O2);

  auto three = getCompilerOptimizationLevel(3);
  ASSERT_TRUE(three.has_value()) << three.error().detail;
  EXPECT_EQ(*three, mqss::mqssci::OptLevel::O3);
}

TEST(GetCompilerOptimizationLevelTest, OutOfRangeLevelIsACompilationError) {
  for (const int level : {-1, 4}) {
    auto result = getCompilerOptimizationLevel(level);
    ASSERT_FALSE(result.has_value()) << "level: " << level;
    EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::CompilationFailed)
        << "level: " << level;
    // The diagnostic must name the offending integer so an operator can tell
    // which task's requested level was rejected.
    EXPECT_NE(result.error().detail.find(std::to_string(level)),
              std::string::npos)
        << "level: " << level << ", detail: " << result.error().detail;
  }
}

// ===========================================================================
// IsOnlineBackendStatusTest
// ===========================================================================
TEST(IsOnlineBackendStatusTest, IdleAndBusyAreOnline) {
  EXPECT_TRUE(isOnlineBackendStatus(mqss::BackendStatus::BACKEND_STATUS_IDLE));
  EXPECT_TRUE(isOnlineBackendStatus(mqss::BackendStatus::BACKEND_STATUS_BUSY));
}

TEST(IsOnlineBackendStatusTest, EveryOtherStatusIsOffline) {
  EXPECT_FALSE(
      isOnlineBackendStatus(mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED));
  EXPECT_FALSE(
      isOnlineBackendStatus(mqss::BackendStatus::BACKEND_STATUS_OFFLINE));
  EXPECT_FALSE(
      isOnlineBackendStatus(mqss::BackendStatus::BACKEND_STATUS_ERROR));
  EXPECT_FALSE(
      isOnlineBackendStatus(mqss::BackendStatus::BACKEND_STATUS_MAINTENANCE));
  EXPECT_FALSE(
      isOnlineBackendStatus(mqss::BackendStatus::BACKEND_STATUS_CALIBRATION));
}

// ===========================================================================
// toQrmciError
// submitter::ErrorCode is generic -- it cannot tell a refused submission from
// a device fault -- so the Kind is the caller's to name and this function only
// formats the detail.
// ===========================================================================
TEST(ToQrmciErrorTest, PassesTheCallersKindThrough) {
  using mqss::submitter::ErrorCode;
  const auto kinds = {Error::Kind::DriverUnavailable, Error::Kind::DeviceError,
                      Error::Kind::SubmissionFailed, Error::Kind::Internal};
  for (const auto kind : kinds) {
    const auto mapped =
        toQrmciError(kind, mqss::submitter::Error{ErrorCode::Fatal, "boom"});
    EXPECT_EQ(mapped.kind, kind);
  }
}

TEST(ToQrmciErrorTest, PreservesTheMessage) {
  // The message is what reaches the log line and the cancellation message
  // sent back to the task's submitter, so it must survive translation.
  const auto mapped =
      toQrmciError(Error::Kind::DeviceError,
                   mqss::submitter::Error{mqss::submitter::ErrorCode::BadState,
                                          "device caught fire"});
  EXPECT_NE(mapped.detail.find("device caught fire"), std::string::npos);
}

TEST(ToQrmciErrorTest, NamesTheErrorCodeInTheDetail) {
  // An ErrorCode does not describe itself, so its name is spelled out in the
  // detail for whoever reads the log.
  const auto mapped =
      toQrmciError(Error::Kind::SubmissionFailed,
                   mqss::submitter::Error{
                       mqss::submitter::ErrorCode::InvalidArgument, "nope"});
  EXPECT_NE(mapped.detail.find("InvalidArgument"), std::string::npos);
}

TEST(ToQrmciErrorTest, NamesEveryErrorCode) {
  // Every enumerator gets a name, so no log line ever reads "Unknown: ..."
  // for a code this build does know about.
  using mqss::submitter::ErrorCode;
  const auto codes = {ErrorCode::Fatal,           ErrorCode::OutOfMem,
                      ErrorCode::NotImplemented,  ErrorCode::LibNotFound,
                      ErrorCode::NotFound,        ErrorCode::OutOfRange,
                      ErrorCode::InvalidArgument, ErrorCode::PermissionDenied,
                      ErrorCode::NotSupported,    ErrorCode::BadState,
                      ErrorCode::Timeout,         ErrorCode::WarnGeneral};
  for (const auto code : codes) {
    const auto mapped = toQrmciError(Error::Kind::DeviceError,
                                     mqss::submitter::Error{code, "detail"});
    EXPECT_EQ(mapped.detail.find("Unknown"), std::string::npos)
        << "unnamed ErrorCode value " << static_cast<int>(code);
  }
}

} // namespace mqss::qrmci::test
