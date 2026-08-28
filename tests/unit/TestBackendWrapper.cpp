/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

namespace mqss::qrmci::test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static BackendWrapper makePopulatedWrapper() {
  BackendWrapper bw;
  bw.setName("test-backend");
  bw.setNumQubits(5);
  bw.setType(mqss::BackendType::BACKEND_TYPE_UNSPECIFIED);
  bw.setStatus(mqss::BackendStatus::BACKEND_STATUS_IDLE);
  bw.setQueueLength(3);
  bw.setCurrentLoad(0.5f);
  bw.setQueueName("test-queue");
  bw.setInstructions({"cx", "h", "measure"});
  bw.setQubitConnectivity({{0, 1}, {1, 2}, {2, 3}});
  bw.setSupportedCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
                                 mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2});
  return bw;
}

static mqss::Backend makeProtoBackend() {
  mqss::Backend backend;
  backend.set_name("proto-backend");
  backend.set_num_qubits(7);
  backend.set_type(mqss::BackendType::BACKEND_TYPE_UNSPECIFIED);
  backend.set_status(mqss::BackendStatus::BACKEND_STATUS_IDLE);
  backend.set_queue_length(2);
  backend.set_current_load(0.25f);
  backend.set_queue_name("proto-queue");
  backend.add_instructions("rx");
  backend.add_instructions("ry");
  auto *qpair = backend.add_connectivity();
  qpair->set_qubit1(0);
  qpair->set_qubit2(1);
  backend.add_supported_circuit_formats(
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3);
  return backend;
}

// ===========================================================================
// BackendWrapperDefaultTest
// ===========================================================================
class BackendWrapperDefaultTest : public ::testing::Test {};

TEST_F(BackendWrapperDefaultTest, NameIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getName().empty());
}

TEST_F(BackendWrapperDefaultTest, QueueNameIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getQueueName().empty());
}

TEST_F(BackendWrapperDefaultTest, InstructionsIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getInstructions().empty());
}

TEST_F(BackendWrapperDefaultTest, ConnectivityIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getQubitConnectivity().empty());
}

TEST_F(BackendWrapperDefaultTest, SSupportedFormatsIsEmpty) {
  BackendWrapper bw;
  EXPECT_TRUE(bw.getSupportedCircuitFormats().empty());
}

// ===========================================================================
// BackendWrapperSetterTest
// ===========================================================================
class BackendWrapperSetterTest : public ::testing::Test {
protected:
  BackendWrapper bw;
};

TEST_F(BackendWrapperSetterTest, setName) {
  bw.setName("my-backend");
  EXPECT_EQ(bw.getName(), "my-backend");
}

TEST_F(BackendWrapperSetterTest, setNumQubits) {
  bw.setNumQubits(10);
  EXPECT_EQ(bw.getNumQubits(), 10u);
}

TEST_F(BackendWrapperSetterTest, setStatus) {
  bw.setStatus(mqss::BackendStatus::BACKEND_STATUS_BUSY);
  EXPECT_EQ(bw.getStatus(), mqss::BackendStatus::BACKEND_STATUS_BUSY);
}

TEST_F(BackendWrapperSetterTest, setQueueLength) {
  bw.setQueueLength(7);
  EXPECT_EQ(bw.getQueueLength(), 7u);
}

TEST_F(BackendWrapperSetterTest, setCurrentLoad) {
  bw.setCurrentLoad(0.75f);
  EXPECT_FLOAT_EQ(bw.getCurrentLoad(), 0.75f);
}

TEST_F(BackendWrapperSetterTest, setQueueName) {
  bw.setQueueName("execution-queue");
  EXPECT_EQ(bw.getQueueName(), "execution-queue");
}

TEST_F(BackendWrapperSetterTest, setInstructions) {
  std::vector<std::string> instrs = {"cx", "h", "t"};
  bw.setInstructions(instrs);
  EXPECT_EQ(bw.getInstructions(), instrs);
}

TEST_F(BackendWrapperSetterTest, setQubitConnectivity) {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> conn = {{0, 1}, {1, 2}};
  bw.setQubitConnectivity(conn);
  EXPECT_EQ(bw.getQubitConnectivity(), conn);
}

TEST_F(BackendWrapperSetterTest, setSupportedCircuitFormats) {
  std::vector<mqss::CircuitFormat> fmts = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  bw.setSupportedCircuitFormats(fmts);
  EXPECT_EQ(bw.getSupportedCircuitFormats(), fmts);
}

// ===========================================================================
// BackendWrapperFromProtoTest
// ===========================================================================
class BackendWrapperFromProtoTest : public ::testing::Test {
protected:
  mqss::Backend proto = makeProtoBackend();
};

TEST_F(BackendWrapperFromProtoTest, NameMatches) {
  BackendWrapper bw(proto);
  EXPECT_EQ(bw.getName(), "proto-backend");
}

TEST_F(BackendWrapperFromProtoTest, NumQubitsMatches) {
  BackendWrapper bw(proto);
  EXPECT_EQ(bw.getNumQubits(), 7u);
}

TEST_F(BackendWrapperFromProtoTest, StatusMatches) {
  BackendWrapper bw(proto);
  EXPECT_EQ(bw.getStatus(), mqss::BackendStatus::BACKEND_STATUS_IDLE);
}

TEST_F(BackendWrapperFromProtoTest, QueueLengthMatches) {
  BackendWrapper bw(proto);
  EXPECT_EQ(bw.getQueueLength(), 2u);
}

TEST_F(BackendWrapperFromProtoTest, CurrentLoadMatches) {
  BackendWrapper bw(proto);
  EXPECT_FLOAT_EQ(bw.getCurrentLoad(), 0.25f);
}

TEST_F(BackendWrapperFromProtoTest, QueueNameMatches) {
  BackendWrapper bw(proto);
  EXPECT_EQ(bw.getQueueName(), "proto-queue");
}

TEST_F(BackendWrapperFromProtoTest, InstructionsMatch) {
  BackendWrapper bw(proto);
  ASSERT_EQ(bw.getInstructions().size(), 2u);
  EXPECT_EQ(bw.getInstructions()[0], "rx");
  EXPECT_EQ(bw.getInstructions()[1], "ry");
}

TEST_F(BackendWrapperFromProtoTest, ConnectivityMatches) {
  BackendWrapper bw(proto);
  ASSERT_EQ(bw.getQubitConnectivity().size(), 1u);
  EXPECT_EQ(bw.getQubitConnectivity()[0].first, 0u);
  EXPECT_EQ(bw.getQubitConnectivity()[0].second, 1u);
}

TEST_F(BackendWrapperFromProtoTest, SupportedCircuitFormatsMatch) {
  BackendWrapper bw(proto);
  ASSERT_EQ(bw.getSupportedCircuitFormats().size(), 1u);
  EXPECT_EQ(bw.getSupportedCircuitFormats()[0],
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3);
}

TEST_F(BackendWrapperFromProtoTest, ConstructFromEmptyBackendMatches) {
  mqss::Backend empty;
  BackendWrapper bw(empty);
  EXPECT_TRUE(bw.getName().empty());
  EXPECT_EQ(bw.getNumQubits(), 0u);
  EXPECT_EQ(bw.getStatus(), mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED);
  EXPECT_EQ(bw.getQueueLength(), 0u);
  EXPECT_FLOAT_EQ(bw.getCurrentLoad(), 0.0f);
  EXPECT_TRUE(bw.getQueueName().empty());
  EXPECT_TRUE(bw.getInstructions().empty());
  EXPECT_TRUE(bw.getQubitConnectivity().empty());
  EXPECT_TRUE(bw.getSupportedCircuitFormats().empty());
}

// ===========================================================================
// BackendWrapperMakeBackendTest
// ===========================================================================
class BackendWrapperMakeBackendTest : public ::testing::Test {
protected:
  BackendWrapper bw = makePopulatedWrapper();
};

TEST_F(BackendWrapperMakeBackendTest, NameMatch) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(backend.name(), bw.getName());
}

TEST_F(BackendWrapperMakeBackendTest, NumQubitsMatch) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(static_cast<std::uint32_t>(backend.num_qubits()),
            bw.getNumQubits());
}

TEST_F(BackendWrapperMakeBackendTest, StatusMatch) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(backend.status(), bw.getStatus());
}

TEST_F(BackendWrapperMakeBackendTest, QueueLengthMatch) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(static_cast<std::uint32_t>(backend.queue_length()),
            bw.getQueueLength());
}

TEST_F(BackendWrapperMakeBackendTest, CurrentLoadMatch) {
  auto backend = bw.makeBackend();
  EXPECT_FLOAT_EQ(backend.current_load(), bw.getCurrentLoad());
}

TEST_F(BackendWrapperMakeBackendTest, QueueNameMatch) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(backend.queue_name(), bw.getQueueName());
}

TEST_F(BackendWrapperMakeBackendTest, InstructionCountMatches) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(static_cast<std::size_t>(backend.instructions().size()),
            bw.getInstructions().size());
}

TEST_F(BackendWrapperMakeBackendTest, ConnectivityCountMatches) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(static_cast<std::size_t>(backend.connectivity().size()),
            bw.getQubitConnectivity().size());
}

TEST_F(BackendWrapperMakeBackendTest, SupportedCircuitFormatsCountMatches) {
  auto backend = bw.makeBackend();
  EXPECT_EQ(
      static_cast<std::size_t>(backend.supported_circuit_formats().size()),
      bw.getSupportedCircuitFormats().size());
}

TEST_F(BackendWrapperMakeBackendTest, ConnectivityFirstPairMatches) {
  auto backend = bw.makeBackend();
  ASSERT_GE(backend.connectivity().size(), 1);
  EXPECT_EQ(static_cast<std::uint32_t>(backend.connectivity(0).qubit1()),
            bw.getQubitConnectivity()[0].first);
  EXPECT_EQ(static_cast<std::uint32_t>(backend.connectivity(0).qubit2()),
            bw.getQubitConnectivity()[0].second);
}

// ===========================================================================
// BackendWrapperCopyMoveTest
// ===========================================================================
class BackendWrapperCopyMoveTest : public ::testing::Test {
protected:
  BackendWrapper src = makePopulatedWrapper();
};

TEST_F(BackendWrapperCopyMoveTest, CopyNamePreserved) {
  BackendWrapper copy(src);
  EXPECT_EQ(copy.getName(), src.getName());
}

TEST_F(BackendWrapperCopyMoveTest, CopyNumQubitsPreserved) {
  BackendWrapper copy(src);
  EXPECT_EQ(copy.getNumQubits(), src.getNumQubits());
}

TEST_F(BackendWrapperCopyMoveTest, CopyIndependentAfterMutation) {
  BackendWrapper copy(src);
  copy.setName("modified");
  EXPECT_NE(copy.getName(), src.getName());
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
