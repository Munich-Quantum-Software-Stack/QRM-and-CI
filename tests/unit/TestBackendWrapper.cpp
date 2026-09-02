/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "BackendWrapperTestBuilder.h"
#include "RunnersTestHelpers.h"
#include "Submitter.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"
#include "qrmci/Error.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

namespace mqss::qrmci::test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static BackendBuilder populatedBuilder() {
  return BackendBuilder()
      .name("test-backend")
      .numQubits(5)
      .type(mqss::BackendType::BACKEND_TYPE_UNSPECIFIED)
      .status(mqss::BackendStatus::BACKEND_STATUS_IDLE)
      .queueLength(3)
      .currentLoad(0.5F)
      .queueName("test-queue")
      .instructions({"cx", "h", "measure"})
      .qubitConnectivity({{0, 1}, {1, 2}, {2, 3}})
      .supportedCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
                                mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2});
}

static BackendBuilder protoBuilder() {
  return BackendBuilder()
      .name("proto-backend")
      .numQubits(7)
      .type(mqss::BackendType::BACKEND_TYPE_UNSPECIFIED)
      .status(mqss::BackendStatus::BACKEND_STATUS_IDLE)
      .queueLength(2)
      .currentLoad(0.25F)
      .queueName("proto-queue")
      .instructions({"rx", "ry"})
      .qubitConnectivity({{0, 1}})
      .supportedCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3});
}

// ===========================================================================
// BackendWrapperDefaultTest
// ===========================================================================
class BackendWrapperDefaultTest : public ::testing::Test {};

TEST_F(BackendWrapperDefaultTest, NameIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getName().empty());
}

TEST_F(BackendWrapperDefaultTest, InstructionsIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getInstructions().empty());
}

TEST_F(BackendWrapperDefaultTest, ConnectivityIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getQubitConnectivity().empty());
}

TEST_F(BackendWrapperDefaultTest, SupportedFormatsIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getSupportedCircuitFormats().empty());
}

TEST_F(BackendWrapperDefaultTest, StatusIsUnspecified) {
  BackendWrapper bw;
  EXPECT_EQ(bw.getStatus(), mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED);
}

TEST_F(BackendWrapperDefaultTest, IsNotOnline) {
  BackendWrapper bw;
  EXPECT_FALSE(bw.isOnline());
}

// ===========================================================================
// BackendWrapperFromProtoTest
// ===========================================================================
class BackendWrapperFromProtoTest : public ::testing::Test {
protected:
  BackendBuilder builder = protoBuilder();
};

TEST_F(BackendWrapperFromProtoTest, NameMatches) {
  EXPECT_EQ(builder.build().getName(), "proto-backend");
}

TEST_F(BackendWrapperFromProtoTest, NumQubitsMatches) {
  EXPECT_EQ(builder.build().getNumQubits(), 7U);
}

TEST_F(BackendWrapperFromProtoTest, StatusMatches) {
  EXPECT_EQ(builder.build().getStatus(),
            mqss::BackendStatus::BACKEND_STATUS_IDLE);
}

TEST_F(BackendWrapperFromProtoTest, InstructionsMatch) {
  const auto bw = builder.build();
  const std::vector<std::string> expected = {"rx", "ry"};
  EXPECT_EQ(bw.getInstructions(), expected);
}

TEST_F(BackendWrapperFromProtoTest, ConnectivityMatches) {
  const auto bw = builder.build();
  const std::vector<std::pair<std::uint32_t, std::uint32_t>> expected = {
      {0, 1}};
  EXPECT_EQ(bw.getQubitConnectivity(), expected);
}

TEST_F(BackendWrapperFromProtoTest, SupportedCircuitFormatsMatch) {
  const auto bw = builder.build();
  const std::vector<mqss::CircuitFormat> expected = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  EXPECT_EQ(bw.getSupportedCircuitFormats(), expected);
}

TEST_F(BackendWrapperFromProtoTest, ConstructFromEmptyBackendMatches) {
  mqss::Backend empty;
  BackendWrapper bw(empty);
  EXPECT_TRUE(bw.getName().empty());
  EXPECT_EQ(bw.getNumQubits(), 0U);
  EXPECT_EQ(bw.getStatus(), mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED);
  EXPECT_TRUE(bw.getInstructions().empty());
  EXPECT_TRUE(bw.getQubitConnectivity().empty());
  EXPECT_TRUE(bw.getSupportedCircuitFormats().empty());
}

TEST_F(BackendWrapperFromProtoTest, OutOfRangeWireFormatBecomesUnspecified) {
  // mqss::Backend arrives from another process, and protobuf keeps an enum
  // value it does not know as its raw integer rather than rejecting the
  // message. The constructor must route it through mapProtoCircuitFormat()
  // so it lands as CIRCUIT_FORMAT_UNSPECIFIED instead of an out-of-range
  // enumerator flowing into the compatibility comparisons.
  const auto bw = BackendBuilder()
                      .name("future-backend")
                      .rawSupportedCircuitFormat(9999)
                      .build();
  ASSERT_EQ(bw.getSupportedCircuitFormats().size(), 1U);
  EXPECT_EQ(bw.getSupportedCircuitFormats()[0],
            mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED);
}

TEST_F(BackendWrapperFromProtoTest, OutOfRangeWireFormatIsNotCompatible) {
  // The unknown format must not accidentally match a task either.
  const auto bw = BackendBuilder()
                      .name("future-backend")
                      .numQubits(5)
                      .status(mqss::BackendStatus::BACKEND_STATUS_IDLE)
                      .rawSupportedCircuitFormat(9999)
                      .build();
  EXPECT_FALSE(bw.canRun(makeTask()));
}

// ===========================================================================
// BackendWrapperToBackendTest
// ===========================================================================
class BackendWrapperToBackendTest : public ::testing::Test {
protected:
  BackendBuilder builder = populatedBuilder();
};

TEST_F(BackendWrapperToBackendTest, RoundTripsEveryField) {
  // toBackend() is the inverse of BackendWrapper(const mqss::Backend &), so
  // wrapping a message and unwrapping it must reproduce it exactly --
  // including queue length, current load, type and queue name, which the
  // wrapper carries for republication but exposes through no getter.
  const mqss::Backend rebuilt = builder.build().toBackend();
  EXPECT_EQ(rebuilt.SerializeAsString(), builder.proto().SerializeAsString());
}

TEST_F(BackendWrapperToBackendTest, IsCallableOnAConstWrapper) {
  // The worker publishes from a backend it has already handed to the
  // registry, so serialising must not require a mutable wrapper.
  const BackendWrapper bw = builder.build();
  EXPECT_EQ(bw.toBackend().name(), "test-backend");
}

TEST_F(BackendWrapperToBackendTest, NameMatch) {
  EXPECT_EQ(builder.build().toBackend().name(), "test-backend");
}

TEST_F(BackendWrapperToBackendTest, InstructionsContentMatches) {
  const auto backend = builder.build().toBackend();
  ASSERT_EQ(backend.instructions().size(), 3);
  EXPECT_EQ(backend.instructions(0), "cx");
  EXPECT_EQ(backend.instructions(1), "h");
  EXPECT_EQ(backend.instructions(2), "measure");
}

TEST_F(BackendWrapperToBackendTest, ConnectivityContentMatches) {
  const auto backend = builder.build().toBackend();
  ASSERT_EQ(backend.connectivity().size(), 3);
  EXPECT_EQ(backend.connectivity(0).qubit1(), 0U);
  EXPECT_EQ(backend.connectivity(0).qubit2(), 1U);
  EXPECT_EQ(backend.connectivity(2).qubit1(), 2U);
  EXPECT_EQ(backend.connectivity(2).qubit2(), 3U);
}

TEST_F(BackendWrapperToBackendTest, SupportedCircuitFormatsContentMatches) {
  const auto backend = builder.build().toBackend();
  ASSERT_EQ(backend.supported_circuit_formats().size(), 2);
  EXPECT_EQ(backend.supported_circuit_formats(0),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3);
  EXPECT_EQ(backend.supported_circuit_formats(1),
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2);
}

// ===========================================================================
// BackendWrapperIsOnlineTest
// ===========================================================================
class BackendWrapperIsOnlineTest : public ::testing::Test {
protected:
  static BackendWrapper withStatus(mqss::BackendStatus status) {
    return BackendBuilder().name("backend").status(status).build();
  }
};

TEST_F(BackendWrapperIsOnlineTest, IdleIsOnline) {
  EXPECT_TRUE(withStatus(mqss::BackendStatus::BACKEND_STATUS_IDLE).isOnline());
}

TEST_F(BackendWrapperIsOnlineTest, BusyIsOnline) {
  EXPECT_TRUE(withStatus(mqss::BackendStatus::BACKEND_STATUS_BUSY).isOnline());
}

TEST_F(BackendWrapperIsOnlineTest, OfflineIsNotOnline) {
  EXPECT_FALSE(
      withStatus(mqss::BackendStatus::BACKEND_STATUS_OFFLINE).isOnline());
}

TEST_F(BackendWrapperIsOnlineTest, MaintenanceIsNotOnline) {
  EXPECT_FALSE(
      withStatus(mqss::BackendStatus::BACKEND_STATUS_MAINTENANCE).isOnline());
}

TEST_F(BackendWrapperIsOnlineTest, CalibrationIsNotOnline) {
  EXPECT_FALSE(
      withStatus(mqss::BackendStatus::BACKEND_STATUS_CALIBRATION).isOnline());
}

TEST_F(BackendWrapperIsOnlineTest, ErrorIsNotOnline) {
  EXPECT_FALSE(
      withStatus(mqss::BackendStatus::BACKEND_STATUS_ERROR).isOnline());
}

// ===========================================================================
// BackendWrapperCanRunTest
// ===========================================================================
class BackendWrapperCanRunTest : public ::testing::Test {
protected:
  BackendWrapper backend = makeBackend("alpha", 5);
};

TEST_F(BackendWrapperCanRunTest, MatchingFormatAndQubitsCanRun) {
  EXPECT_TRUE(backend.canRun(makeTask("OPENQASM 3.0;", 5, "qasm3")));
}

TEST_F(BackendWrapperCanRunTest, TooManyQubitsCannotRun) {
  EXPECT_FALSE(backend.canRun(makeTask("OPENQASM 3.0;", 6, "qasm3")));
}

TEST_F(BackendWrapperCanRunTest, NegativeQubitCountCannotRun) {
  // n_qbits is a signed field on the wire, so a negative value must be
  // rejected rather than wrapping around into a huge unsigned count.
  auto task = makeTask();
  task.set_n_qbits(-1);
  EXPECT_FALSE(backend.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, UnsupportedCircuitTypeCannotRun) {
  EXPECT_FALSE(backend.canRun(makeTask("OPENQASM 2.0;", 2, "qasm2")));
}

TEST_F(BackendWrapperCanRunTest, UnknownCircuitTypeCannotRun) {
  EXPECT_FALSE(backend.canRun(makeTask("something", 2, "not-a-format")));
}

TEST_F(BackendWrapperCanRunTest, AnyMatchingFormatIsEnough) {
  // "qir" maps to five circuit formats; a backend supporting just one of
  // them can still run the task.
  const auto qirBackend =
      makeBackend("qir-backend", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
                   mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE});
  EXPECT_TRUE(qirBackend.canRun(makeTask("circuit", 2, "qir")));
}

// ===========================================================================
// BackendWrapperCompilerResultFormatTest
// ===========================================================================
class BackendWrapperCompilerResultFormatTest : public ::testing::Test {};

TEST_F(BackendWrapperCompilerResultFormatTest, MappedFormatIsReturned) {
  const auto backend =
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
  const auto resultFormat = backend.compilerResultFormat();
  ASSERT_TRUE(resultFormat.has_value());
  EXPECT_EQ(*resultFormat, mqss::mqssci::ResultFormat::QIRBASE);
}

TEST_F(BackendWrapperCompilerResultFormatTest, FirstMappedFormatWins) {
  // QASM3 has no result-format mapping, so the scan must skip it and take
  // the next format the compiler can actually emit.
  const auto backend =
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
                   mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2});
  const auto resultFormat = backend.compilerResultFormat();
  ASSERT_TRUE(resultFormat.has_value());
  EXPECT_EQ(*resultFormat, mqss::mqssci::ResultFormat::OPENQASM2);
}

TEST_F(BackendWrapperCompilerResultFormatTest, NoMappedFormatFails) {
  const auto backend =
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3});
  const auto resultFormat = backend.compilerResultFormat();
  ASSERT_FALSE(resultFormat.has_value());
  // No format this backend supports can ever be emitted by the compiler, so
  // the failure is permanent rather than something to retry.
  EXPECT_EQ(resultFormat.error().kind,
            mqss::qrmci::Error::Kind::UnsupportedFormat);
  EXPECT_FALSE(resultFormat.error().isRetryable());
  EXPECT_FALSE(resultFormat.error().detail.empty());
}

TEST_F(BackendWrapperCompilerResultFormatTest, NoFormatsAtAllFails) {
  EXPECT_FALSE(BackendWrapper().compilerResultFormat().has_value());
}

// ===========================================================================
// BackendWrapperCopyMoveTest
// ===========================================================================
class BackendWrapperCopyMoveTest : public ::testing::Test {
protected:
  BackendWrapper src = populatedBuilder().build();
};

TEST_F(BackendWrapperCopyMoveTest, CopyNamePreserved) {
  BackendWrapper copy(src);
  EXPECT_EQ(copy.getName(), src.getName());
}

TEST_F(BackendWrapperCopyMoveTest, CopyNumQubitsPreserved) {
  BackendWrapper copy(src);
  EXPECT_EQ(copy.getNumQubits(), src.getNumQubits());
}

TEST_F(BackendWrapperCopyMoveTest, CopyInstructionsPreserved) {
  BackendWrapper copy(src);
  EXPECT_EQ(copy.getInstructions(), src.getInstructions());
}

TEST_F(BackendWrapperCopyMoveTest, MoveNamePreserved) {
  const std::string expectedName = src.getName();
  BackendWrapper moved(std::move(src));
  EXPECT_EQ(moved.getName(), expectedName);
}

TEST_F(BackendWrapperCopyMoveTest, MoveNumQubitsPreserved) {
  const auto expectedQubits = src.getNumQubits();
  BackendWrapper moved(std::move(src));
  EXPECT_EQ(moved.getNumQubits(), expectedQubits);
}

TEST_F(BackendWrapperCopyMoveTest, CopyAssignmentNamePreserved) {
  BackendWrapper copy;
  copy = src;
  EXPECT_EQ(copy.getName(), src.getName());
}

TEST_F(BackendWrapperCopyMoveTest,
       CopyAssignmentSupportedCircuitFormatsPreserved) {
  BackendWrapper copy;
  copy = src;
  EXPECT_EQ(copy.getSupportedCircuitFormats(),
            src.getSupportedCircuitFormats());
}

TEST_F(BackendWrapperCopyMoveTest, MoveAssignmentNamePreserved) {
  const std::string expectedName = src.getName();
  BackendWrapper moved;
  moved = std::move(src);
  EXPECT_EQ(moved.getName(), expectedName);
}

// ===========================================================================
// BackendWrapperFromSubmitterTest
// Exercises the Submitter-based constructor end-to-end against the real
// example QDMI driver. The QDMI_Device_Status -> BackendStatus and
// QDMI_Program_Format -> CircuitFormat mapping themselves (mapBackendStatus /
// mapCircuitFormat) are unit-tested directly, without a live driver, in
// TestConstantsMapping.cpp; this fixture instead confirms the constructor
// wires a real device's values through correctly. Mirrors the fixture used by
// TestRunnersTaskExecution, which requires the same QDMI_CONF /
// LD_LIBRARY_PATH environment set up in CMakeLists.txt.
// ===========================================================================
class BackendWrapperFromSubmitterTest : public ::testing::Test {
protected:
  mqss::submitter::Submitter submitter = mqss::submitter::Submitter(
      "qdmi_example_driver", "C++ Device with 5 qubits",
      "C++ Device with 5 qubits", "example_token");
};

TEST_F(BackendWrapperFromSubmitterTest, NameMatchesDeviceID) {
  BackendWrapper bw(submitter);
  EXPECT_EQ(bw.getName(), "C++ Device with 5 qubits");
}

TEST_F(BackendWrapperFromSubmitterTest, NumQubitsMatchesDevice) {
  BackendWrapper bw(submitter);
  EXPECT_EQ(bw.getNumQubits(), 5U);
}

TEST_F(BackendWrapperFromSubmitterTest, StatusIsMappedToIdle) {
  // The example device is online and ready to accept jobs, so
  // mapBackendStatus() must translate its QDMI status into
  // BACKEND_STATUS_IDLE rather than falling through to the UNSPECIFIED
  // default case.
  BackendWrapper bw(submitter);
  EXPECT_EQ(bw.getStatus(), mqss::BackendStatus::BACKEND_STATUS_IDLE);
  EXPECT_TRUE(bw.isOnline());
}

TEST_F(BackendWrapperFromSubmitterTest, InstructionsAreNotEmpty) {
  BackendWrapper bw(submitter);
  EXPECT_FALSE(bw.getInstructions().empty());
}

TEST_F(BackendWrapperFromSubmitterTest, SupportedCircuitFormatsAreMapped) {
  // mapCircuitFormat() must translate every QDMI_Program_Format the device
  // reports into a known, non-default mqss::CircuitFormat.
  BackendWrapper bw(submitter);
  ASSERT_FALSE(bw.getSupportedCircuitFormats().empty());
  for (const auto &format : bw.getSupportedCircuitFormats()) {
    EXPECT_NE(format, mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED);
  }
}

TEST_F(BackendWrapperFromSubmitterTest, RoundTripsThroughToBackend) {
  // A worker publishes its live device this way, and the selector rebuilds
  // it from the message; the two views must agree.
  BackendWrapper bw(submitter);
  const BackendWrapper republished(bw.toBackend());
  EXPECT_EQ(republished.getName(), bw.getName());
  EXPECT_EQ(republished.getNumQubits(), bw.getNumQubits());
  EXPECT_EQ(republished.getStatus(), bw.getStatus());
  EXPECT_EQ(republished.getInstructions(), bw.getInstructions());
  EXPECT_EQ(republished.getQubitConnectivity(), bw.getQubitConnectivity());
  EXPECT_EQ(republished.getSupportedCircuitFormats(),
            bw.getSupportedCircuitFormats());
}

TEST_F(BackendWrapperFromSubmitterTest, ConnectivityMatchesDevice) {
  // RoundTripsThroughToBackend only compares a wrapper's connectivity to its
  // own round-trip, so it can't catch a constructor that drops connectivity
  // entirely; this compares directly against what the device itself reports.
  BackendWrapper bw(submitter);
  const auto devicePairs = submitter.getDeviceConnectivity();
  std::vector<std::pair<std::uint32_t, std::uint32_t>> expected;
  expected.reserve(devicePairs.size());
  for (const auto &pair : devicePairs) {
    expected.emplace_back(pair.first, pair.second);
  }
  EXPECT_EQ(bw.getQubitConnectivity(), expected);
}

} // namespace mqss::qrmci::test
