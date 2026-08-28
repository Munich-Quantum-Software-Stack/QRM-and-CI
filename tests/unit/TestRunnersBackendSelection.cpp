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
static mqss::qrmci::BackendWrapper makeBackend(
    const std::string &name, std::uint32_t numQubits = 5,
    mqss::BackendStatus status = mqss::BackendStatus::BACKEND_STATUS_IDLE,
    const std::vector<mqss::CircuitFormat> &formats = {
        mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3}) {
  mqss::qrmci::BackendWrapper bw;
  bw.setName(name);
  bw.setNumQubits(numQubits);
  bw.setStatus(status);
  bw.setInstructions({"rx, cz, measure"});
  bw.setSupportedCircuitFormats(formats);
  return bw;
}

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
// selectBackend
// ===========================================================================
class SelectBackendTest : public ::testing::Test {
protected:
  std::unordered_map<std::string, mqss::qrmci::BackendWrapper> backends;

  void SetUp() override {
    backends["alpha"] = makeBackend("alpha", 5);
    backends["beta"] = makeBackend("beta", 10);
  }
};

TEST_F(SelectBackendTest, EmptyAvailableBackend) {
  std::unordered_map<std::string, mqss::qrmci::BackendWrapper> empty;
  auto task = makeTask();
  auto result = mqss::qrmci::selectBackend(task, empty);
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(result.error().empty());
}

TEST_F(SelectBackendTest, PreferredBackendExists) {
  auto task = makeTask("OPENQASM 3.0;", 7);
  task.set_preferred_qpu("beta");
  auto result = mqss::qrmci::selectBackend(task, backends);
  EXPECT_TRUE(result.has_value());
}

TEST_F(SelectBackendTest, TaskRequiresMoreQubitsThanAllBackends) {
  auto task = makeTask("OPENQASM 3.0;", 64); // exceeds both backends
  auto result = mqss::qrmci::selectBackend(task, backends);
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(result.error().empty());
}

TEST_F(SelectBackendTest, AllBackendsOffline) {
  backends.clear();
  backends["offline"] =
      makeBackend("offline", 5, mqss::BackendStatus::BACKEND_STATUS_OFFLINE);
  auto task = makeTask();
  task.set_preferred_qpu("offline");
  auto result = mqss::qrmci::selectBackend(task, backends);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SelectBackendTest, CircuitFormatUnsupportedByBackends) {
  backends.clear();
  backends["qasm2only"] =
      makeBackend("qasm2only", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2});
  auto task = makeTask("OPENQASM 3.0;", 2, "qasm3");
  task.set_preferred_qpu("qasm2only");
  auto result = mqss::qrmci::selectBackend(task, backends);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SelectBackendTest, CompatibleBackendSelectedFromMultipleOptions) {
  auto task = makeTask();
  task.set_preferred_qpu("alpha");
  auto result = mqss::qrmci::selectBackend(task, backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(task.scheduled_qpu().empty());
}

} // namespace mqss::qrmci::test
