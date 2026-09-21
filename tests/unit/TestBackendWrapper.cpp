/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "BackendWrapperTestBuilder.h"
#include "RunnersTestHelpers.h"
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
  const auto bw = BackendBuilder().build();
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
  auto task = makeTask();
  task.set_no_modify(true);
  EXPECT_FALSE(bw.canRun(task));
}

// ===========================================================================
// BackendWrapperFromPublishedStatusTest
// ===========================================================================
class BackendWrapperFromPublishedStatusTest : public ::testing::Test {};

TEST_F(BackendWrapperFromPublishedStatusTest, EmptyNameIsRejected) {
  const auto backend = BackendBuilder().queueName("queue").proto();
  auto result = BackendWrapper::fromPublishedStatus(backend);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
}

TEST_F(BackendWrapperFromPublishedStatusTest, EmptyQueueNameIsRejected) {
  const auto backend = BackendBuilder().name("alpha").proto();
  auto result = BackendWrapper::fromPublishedStatus(backend);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
}

TEST_F(BackendWrapperFromPublishedStatusTest, ValidStatusIsAccepted) {
  const auto backend =
      BackendBuilder().name("alpha").queueName("alpha-queue").proto();
  auto result = BackendWrapper::fromPublishedStatus(backend);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getName(), "alpha");
  EXPECT_EQ(result->getQueueName(), "alpha-queue");
}

TEST_F(BackendWrapperFromPublishedStatusTest,
       UnknownWireTypeNormalizesToUnspecified) {
  const auto backend = BackendBuilder()
                           .name("alpha")
                           .queueName("alpha-queue")
                           .rawType(9999)
                           .proto();
  auto result = BackendWrapper::fromPublishedStatus(backend);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->toBackend().type(),
            mqss::BackendType::BACKEND_TYPE_UNSPECIFIED);
}

TEST_F(BackendWrapperFromPublishedStatusTest,
       UnknownWireStatusNormalizesToUnspecifiedAndOffline) {
  const auto backend = BackendBuilder()
                           .name("alpha")
                           .queueName("alpha-queue")
                           .rawStatus(9999)
                           .proto();
  auto result = BackendWrapper::fromPublishedStatus(backend);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getStatus(),
            mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED);
  EXPECT_FALSE(result->isOnline());
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
  // Direct (no-modify) submission: "qasm3" maps straight to
  // CIRCUIT_FORMAT_QASM3, which the fixture backend supports.
  auto task = makeTask("OPENQASM 3.0;", 5, "qasm3");
  task.set_no_modify(true);
  EXPECT_TRUE(backend.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, TooManyQubitsCannotRun) {
  auto task = makeTask("OPENQASM 3.0;", 6, "qasm3");
  task.set_no_modify(true);
  EXPECT_FALSE(backend.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, NegativeQubitCountCannotRun) {
  // n_qbits is a signed field on the wire, so a negative value must be
  // rejected rather than wrapping around into a huge unsigned count.
  auto task = makeTask();
  task.set_n_qbits(-1);
  EXPECT_FALSE(backend.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, UnsupportedCircuitTypeCannotRun) {
  // Direct submission of "qasm2" needs CIRCUIT_FORMAT_QASM2, which the
  // fixture backend (QASM3 only) does not support.
  auto task = makeTask("OPENQASM 2.0;", 2, "qasm2");
  task.set_no_modify(true);
  EXPECT_FALSE(backend.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, UnknownCircuitTypeCannotRun) {
  EXPECT_FALSE(backend.canRun(makeTask("something", 2, "not-a-format")));
}

TEST_F(BackendWrapperCanRunTest, NoModifyFalseWithNonCompilerInputCannotRun) {
  // "qasm3" has no compiler input mapping, so with no_modify left at its
  // default (false) admission must fail here rather than succeeding and
  // failing later at compilation -- even though the backend does support
  // CIRCUIT_FORMAT_QASM3 directly.
  EXPECT_FALSE(backend.canRun(makeTask("OPENQASM 3.0;", 5, "qasm3")));
}

TEST_F(BackendWrapperCanRunTest, CompilableInputWithCompilerTargetCanRun) {
  // "quake" is a compiler input; a backend offering CIRCUIT_FORMAT_QASM2
  // gives the compiler a target to compile it for.
  const auto compilableBackend =
      makeBackend("compilable", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2});
  EXPECT_TRUE(compilableBackend.canRun(makeTask("circuit", 2, "quake")));
}

TEST_F(BackendWrapperCanRunTest,
       CompilableInputWithoutCompilerTargetCannotRun) {
  // The backend only offers formats the compiler cannot emit, so there is no
  // compiler target to compile "quake" for.
  const auto uncompilableBackend =
      makeBackend("uncompilable", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3});
  EXPECT_FALSE(uncompilableBackend.canRun(makeTask("circuit", 2, "quake")));
}

TEST_F(BackendWrapperCanRunTest, ExactDirectFormatIsRequired) {
  // Previously a task with circuit type "qir" could run on a backend
  // supporting any one of five distinct circuit formats. The policy is now
  // deterministic: "qir" resolves to exactly CIRCUIT_FORMAT_QIRBASESTRING,
  // so a backend supporting only a different QIR variant is not a match.
  const auto qirAdaptiveModuleOnly =
      makeBackend("qir-backend", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
                   mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE});
  auto task = makeTask("circuit", 2, "qir");
  task.set_no_modify(true);
  EXPECT_FALSE(qirAdaptiveModuleOnly.canRun(task));

  const auto qirBaseString =
      makeBackend("qir-backend-2", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
  EXPECT_TRUE(qirBaseString.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, EmptyRestrictionListIsUnrestricted) {
  auto task = makeTask("OPENQASM 3.0;", 5, "qasm3");
  task.set_no_modify(true);
  ASSERT_TRUE(task.restricted_resource_names().empty());
  EXPECT_TRUE(backend.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, NameInRestrictionListCanRun) {
  auto task = makeTask("OPENQASM 3.0;", 5, "qasm3");
  task.set_no_modify(true);
  task.add_restricted_resource_names("alpha");
  task.add_restricted_resource_names("beta");
  EXPECT_TRUE(backend.canRun(task));
}

TEST_F(BackendWrapperCanRunTest, NameOutsideRestrictionListCannotRun) {
  auto task = makeTask("OPENQASM 3.0;", 5, "qasm3");
  task.set_no_modify(true);
  task.add_restricted_resource_names("beta");
  task.add_restricted_resource_names("gamma");
  EXPECT_FALSE(backend.canRun(task));
}

// ===========================================================================
// BackendWrapperCompilerTargetTest
// ===========================================================================
class BackendWrapperCompilerTargetTest : public ::testing::Test {};

TEST_F(BackendWrapperCompilerTargetTest, MappedFormatIsReturned) {
  const auto backend =
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
  const auto target = backend.compilerTarget();
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->compilerFormat, mqss::mqssci::ResultFormat::QIRBASE);
  EXPECT_EQ(target->circuitFormat,
            mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING);
  EXPECT_EQ(target->taskCircuitType, "qirbase");
}

TEST_F(BackendWrapperCompilerTargetTest, PriorityOrderWinsOverFormatOrder) {
  // QASM3 has no compiler target, so the fixed priority order must skip it
  // and pick QASM2 -- the backend's own format list order is irrelevant.
  const auto backend =
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
                   mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2});
  const auto target = backend.compilerTarget();
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->compilerFormat, mqss::mqssci::ResultFormat::OPENQASM2);
}

TEST_F(BackendWrapperCompilerTargetTest, NoMappedFormatFails) {
  const auto backend =
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3});
  const auto target = backend.compilerTarget();
  ASSERT_FALSE(target.has_value());
  // No format this backend supports can ever be emitted by the compiler.
  EXPECT_EQ(target.error().kind, mqss::qrmci::Error::Kind::UnsupportedFormat);
  EXPECT_FALSE(target.error().detail.empty());
}

TEST_F(BackendWrapperCompilerTargetTest, NoFormatsAtAllFails) {
  EXPECT_FALSE(BackendWrapper().compilerTarget().has_value());
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

} // namespace mqss::qrmci::test
