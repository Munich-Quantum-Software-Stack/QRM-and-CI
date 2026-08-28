/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"
#include "qrmci/Runners.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mqss::qrmci::test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static mqss::QuantumTask makeTask(const std::string &circuit = "OPENQASM 3.0;",
                                  std::uint32_t numQubits = 2,
                                  std::string type = "qasm3") {
  mqss::QuantumTask task;
  task.add_circuit_files({circuit});
  task.set_n_qbits(static_cast<int>(numQubits));
  task.set_n_shots(10);
  task.set_circuit_file_type(type);
  return task;
}

// ===========================================================================
// executeQuantumTask
// ===========================================================================
class ExecuteQuantumTaskTest : public ::testing::Test {
public:
  ExecuteQuantumTaskTest() = default;

protected:
  // QDMI_CONF is set in CMakeLists.txt to point to the qdmi.conf file generated
  // during the build process. It contains the path to the example device
  // cxx-qdmi-device shared library from QDMI repo.
  mqss::submitter::Submitter submitter = mqss::submitter::Submitter(
      "qdmi_example_driver", "C++ Device with 5 qubits",
      "C++ Device with 5 qubits", "example_token");

  mqss::QuantumTask task;

  void SetUp() override { task = makeTask(); }
};

TEST_F(ExecuteQuantumTaskTest, ValidTaskWithValidSubmitter) {
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->execution_status(), true);
}

TEST_F(ExecuteQuantumTaskTest, EmptyCircuitFilesWithValidSubmitter) {
  task.clear_circuit_files();
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  EXPECT_FALSE(result.has_value());
  EXPECT_TRUE(!result.error().empty());
}

TEST_F(ExecuteQuantumTaskTest, ZeroNumShotsWithValidSubmitter) {
  task.set_n_shots(0);
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  EXPECT_FALSE(result.has_value());
  EXPECT_TRUE(!result.error().empty());
}

// ===========================================================================
// cancelQuantumTask
// ===========================================================================
class CancelQuantumTaskTest : public ::testing::Test {
protected:
  mqss::QuantumTask task;

  void SetUp() override {
    task = makeTask();
    task.set_task_id(1);
  }
};

TEST_F(CancelQuantumTaskTest, ValidReasonReturnsCancelledResult) {
  const std::string reason = "User requested cancellation";
  auto result = mqss::qrmci::cancelQuantumTask(task, reason);
  EXPECT_EQ(result.execution_status(), false);
}

TEST_F(CancelQuantumTaskTest, ValidReasonResultContainsReason) {
  const std::string reason = "Timeout exceeded";
  auto result = mqss::qrmci::cancelQuantumTask(task, reason);
  EXPECT_THAT(result.additional_information(), ::testing::HasSubstr(reason));
}

TEST_F(CancelQuantumTaskTest, EmptyReasonStillReturnsCancelledStatus) {
  auto result = mqss::qrmci::cancelQuantumTask(task, "");
  EXPECT_EQ(result.execution_status(), false);
}

TEST_F(CancelQuantumTaskTest, PreservesTaskId) {
  auto result = mqss::qrmci::cancelQuantumTask(task, "some reason");
  EXPECT_EQ(result.task_id(), task.task_id());
}

} // namespace mqss::qrmci::test
