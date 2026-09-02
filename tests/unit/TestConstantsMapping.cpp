/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/Protocol.hpp"
#include "qdmi/constants.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <array>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mqss::qrmci::test {

// ===========================================================================
// MapBackendStatusTest
// ===========================================================================
TEST(MapBackendStatusTest, Offline) {
  EXPECT_EQ(mapBackendStatus(QDMI_DEVICE_STATUS_OFFLINE),
            mqss::BackendStatus::BACKEND_STATUS_OFFLINE);
}

TEST(MapBackendStatusTest, Idle) {
  EXPECT_EQ(mapBackendStatus(QDMI_DEVICE_STATUS_IDLE),
            mqss::BackendStatus::BACKEND_STATUS_IDLE);
}

TEST(MapBackendStatusTest, Busy) {
  EXPECT_EQ(mapBackendStatus(QDMI_DEVICE_STATUS_BUSY),
            mqss::BackendStatus::BACKEND_STATUS_BUSY);
}

TEST(MapBackendStatusTest, Error) {
  EXPECT_EQ(mapBackendStatus(QDMI_DEVICE_STATUS_ERROR),
            mqss::BackendStatus::BACKEND_STATUS_ERROR);
}

TEST(MapBackendStatusTest, Maintenance) {
  EXPECT_EQ(mapBackendStatus(QDMI_DEVICE_STATUS_MAINTENANCE),
            mqss::BackendStatus::BACKEND_STATUS_MAINTENANCE);
}

TEST(MapBackendStatusTest, Calibration) {
  EXPECT_EQ(mapBackendStatus(QDMI_DEVICE_STATUS_CALIBRATION),
            mqss::BackendStatus::BACKEND_STATUS_CALIBRATION);
}

TEST(MapBackendStatusTest, UnknownFallsBackToUnspecified) {
  // QDMI_DEVICE_STATUS_MAX is a real enum value with no mapping case; the
  // function must fall through to the default rather than misclassify it.
  EXPECT_EQ(mapBackendStatus(QDMI_DEVICE_STATUS_MAX),
            mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED);
}

// ===========================================================================
// MapCircuitFormatTest
// ===========================================================================
TEST(MapCircuitFormatTest, Qasm2) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_QASM2),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2);
}

TEST(MapCircuitFormatTest, Qasm3) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_QASM3),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3);
}

TEST(MapCircuitFormatTest, QirBaseString) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_QIRBASESTRING),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING);
}

TEST(MapCircuitFormatTest, QirBaseModule) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_QIRBASEMODULE),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE);
}

TEST(MapCircuitFormatTest, QirAdaptiveString) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_QIRADAPTIVESTRING),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING);
}

TEST(MapCircuitFormatTest, QirAdaptiveModule) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_QIRADAPTIVEMODULE),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE);
}

TEST(MapCircuitFormatTest, Calibration) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_CALIBRATION),
            mqss::CircuitFormat::CIRCUIT_FORMAT_CALIBRATION);
}

TEST(MapCircuitFormatTest, Qpy) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_QPY),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QPY);
}

TEST(MapCircuitFormatTest, IqmJson) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_IQMJSON),
            mqss::CircuitFormat::CIRCUIT_FORMAT_IQMJSON);
}

TEST(MapCircuitFormatTest, BatchJob) {
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_BATCHJOB),
            mqss::CircuitFormat::CIRCUIT_FORMAT_BATCHJOB);
}

TEST(MapCircuitFormatTest, UnknownFallsBackToUnspecified) {
  // QDMI_PROGRAM_FORMAT_MAX is a real enum value with no mapping case; the
  // function must fall through to the default rather than misclassify it.
  EXPECT_EQ(mapCircuitFormat(QDMI_PROGRAM_FORMAT_MAX),
            mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED);
}

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
// GetCompilerOptimizationLevelTest
// ===========================================================================
TEST(GetCompilerOptimizationLevelTest, MappedLevels) {
  EXPECT_EQ(getCompilerOptimizationLevel(1), mqss::mqssci::OptLevel::O1);
  EXPECT_EQ(getCompilerOptimizationLevel(2), mqss::mqssci::OptLevel::O2);
  EXPECT_EQ(getCompilerOptimizationLevel(3), mqss::mqssci::OptLevel::O3);
}

TEST(GetCompilerOptimizationLevelTest, UnmappedLevelDefaultsToO3) {
  EXPECT_EQ(getCompilerOptimizationLevel(0), mqss::mqssci::OptLevel::O3);
  EXPECT_EQ(getCompilerOptimizationLevel(-1), mqss::mqssci::OptLevel::O3);
  EXPECT_EQ(getCompilerOptimizationLevel(42), mqss::mqssci::OptLevel::O3);
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
// MapCircuitFormatToResultFormatTest
// ===========================================================================
TEST(MapCircuitFormatToResultFormatTest, MappedFormats) {
  EXPECT_EQ(
      mapCircuitFormatToResultFormat(mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2),
      mqss::mqssci::ResultFormat::OPENQASM2);
  EXPECT_EQ(
      mapCircuitFormatToResultFormat(mqss::CircuitFormat::CIRCUIT_FORMAT_QIR),
      mqss::mqssci::ResultFormat::QIR);
  EXPECT_EQ(mapCircuitFormatToResultFormat(
                mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING),
            mqss::mqssci::ResultFormat::QIRBASE);
  EXPECT_EQ(mapCircuitFormatToResultFormat(
                mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE),
            mqss::mqssci::ResultFormat::QIRBASE);
  EXPECT_EQ(mapCircuitFormatToResultFormat(
                mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING),
            mqss::mqssci::ResultFormat::QIRADAPTIVE);
  EXPECT_EQ(mapCircuitFormatToResultFormat(
                mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE),
            mqss::mqssci::ResultFormat::QIRADAPTIVE);
}

TEST(MapCircuitFormatToResultFormatTest, UnmappedFormatsHaveNoResultFormat) {
  // The compiler cannot emit these, so a backend offering only them has no
  // usable result format.
  EXPECT_FALSE(
      mapCircuitFormatToResultFormat(mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3)
          .has_value());
  EXPECT_FALSE(mapCircuitFormatToResultFormat(
                   mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED)
                   .has_value());
  EXPECT_FALSE(
      mapCircuitFormatToResultFormat(mqss::CircuitFormat::CIRCUIT_FORMAT_QPY)
          .has_value());
}

// ===========================================================================
// IsCircuitTypeCompatibleWithFormatsTest
// ===========================================================================
TEST(IsCircuitTypeCompatibleWithFormatsTest, DirectMatch) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  EXPECT_TRUE(isCircuitTypeCompatibleWithFormats("qasm3", formats));
}

TEST(IsCircuitTypeCompatibleWithFormatsTest, AnyOfSeveralMappedFormatsMatches) {
  // "qir" maps to five circuit formats; supporting any one of them is enough.
  const std::array formats = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE};
  EXPECT_TRUE(isCircuitTypeCompatibleWithFormats("qir", formats));
}

TEST(IsCircuitTypeCompatibleWithFormatsTest, NonMatchingFormatIsRejected) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2};
  EXPECT_FALSE(isCircuitTypeCompatibleWithFormats("qasm3", formats));
}

TEST(IsCircuitTypeCompatibleWithFormatsTest, UnknownCircuitTypeIsRejected) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  EXPECT_FALSE(isCircuitTypeCompatibleWithFormats("not-a-type", formats));
  EXPECT_FALSE(isCircuitTypeCompatibleWithFormats("", formats));
}

TEST(IsCircuitTypeCompatibleWithFormatsTest, EmptyFormatListIsRejected) {
  EXPECT_FALSE(isCircuitTypeCompatibleWithFormats("qasm3", {}));
}

// ===========================================================================
// ValidateInputFormatIsSupportedCircuitFormatTest
// ===========================================================================
TEST(ValidateInputFormatIsSupportedCircuitFormatTest, CompilerFormatsAccepted) {
  const std::vector<std::string_view> supported =
      mqss::mqssci::MQSSCompiler::getSupportedInputFormats();
  EXPECT_TRUE(validateInputFormatIsSupportedCircuitFormat("quake", supported)
                  .has_value());
  EXPECT_TRUE(validateInputFormatIsSupportedCircuitFormat("catalyst", supported)
                  .has_value());
}

TEST(ValidateInputFormatIsSupportedCircuitFormatTest, UnknownTypeIsRejected) {
  const std::vector<std::string_view> supported =
      mqss::mqssci::MQSSCompiler::getSupportedInputFormats();
  auto result = validateInputFormatIsSupportedCircuitFormat("qasm3", supported);
  ASSERT_FALSE(result.has_value());
  // Permanent, and the submitter's input is at fault: the same format will be
  // rejected on every retry.
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::UnsupportedFormat);
  EXPECT_FALSE(result.error().isRetryable());
  EXPECT_NE(result.error().detail.find("qasm3"), std::string::npos);
}

TEST(ValidateInputFormatIsSupportedCircuitFormatTest,
     KnownTypeTheCompilerDoesNotAcceptIsRejected) {
  // "quake" maps to "cudaq-quake", which is absent from this compiler's
  // supported input formats.
  const std::array<std::string_view, 1> supported = {"catalyst-quantum"};
  EXPECT_FALSE(validateInputFormatIsSupportedCircuitFormat("quake", supported)
                   .has_value());
}

TEST(ValidateInputFormatIsSupportedCircuitFormatTest, EmptyInputIsRejected) {
  const std::vector<std::string_view> supported =
      mqss::mqssci::MQSSCompiler::getSupportedInputFormats();
  EXPECT_FALSE(
      validateInputFormatIsSupportedCircuitFormat("", supported).has_value());
}

} // namespace mqss::qrmci::test
