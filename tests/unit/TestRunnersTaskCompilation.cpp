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
// compileQuantumTask
// ===========================================================================
class CompileQuantumTaskTest : public ::testing::Test {
protected:
  mqss::qrmci::BackendWrapper backend;

  void SetUp() override { backend = makeBackend("alpha", 5); }
};

TEST_F(CompileQuantumTaskTest, ValidTaskAndBackend) {
  auto task = makeTask();
  task.set_scheduled_qpu(backend.getName());
  task.set_circuit_file_type("quake");
  task.clear_circuit_files();
  task.add_circuit_files(
      R"(
      module {
      func.func @hadamard_circuit() {
        %q0 = quake.alloca !quake.ref
        quake.h %q0 : (!quake.ref) -> ()
        %b0 = quake.mz %q0 : (!quake.ref) -> !quake.measure
        return
      }
    }
  )");
  backend.setSupportedCircuitFormats(
      {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING,
       mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3});
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_TRUE(result.has_value());
}

TEST_F(CompileQuantumTaskTest, EmptyCircuit) {
  auto task = makeTask("" /* empty circuit */);
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(result.error().empty());
}

TEST_F(CompileQuantumTaskTest, InvalidCircuitSyntax) {
  auto task = makeTask("THIS IS NOT VALID QASM3 SYNTAX;");
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_FALSE(result.has_value());
}

} // namespace mqss::qrmci::test
