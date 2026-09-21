/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "RunnersTestHelpers.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"
#include "qrmci/Error.h"
#include "qrmci/Runners.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mqss::qrmci::test {

// ===========================================================================
// compileQuantumTask
// ===========================================================================
class CompileQuantumTaskTest : public ::testing::Test {
protected:
  mqss::qrmci::BackendWrapper backend;

  void SetUp() override { backend = makeBackend("alpha", 5); }

  /// @brief Rebuild the fixture backend supporting exactly @p formats.
  ///        BackendWrapper has no setters, so a test that needs different
  ///        circuit formats builds a new backend instead of mutating one.
  void useCircuitFormats(const std::vector<mqss::CircuitFormat> &formats) {
    backend = makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                          formats);
  }
};

TEST_F(CompileQuantumTaskTest, FixtureBackendReportsThreeInstructions) {
  // Regression guard for the malformed fixture data in RunnersTestHelpers.h
  // (R10): instructions({"rx, cz, measure"}) would set one instruction named
  // "rx, cz, measure" rather than three separate instructions.
  EXPECT_EQ(backend.getInstructions().size(), 3);
}

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
  useCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING,
                     mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3});
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  ASSERT_TRUE(result.has_value());
  // The compiled bytes no longer match "quake", so the task's circuit type
  // must be relabeled with the compiler target's own canonical label --
  // here "qirbase", since QIRBASESTRING is what the backend offers.
  EXPECT_EQ(task.circuit_file_type(), "qirbase");
}

TEST_F(CompileQuantumTaskTest,
       UnsupportedCircuitFileTypeFailsFormatValidation) {
  // "qasm3" is not a key in TaskCircuitTypeToCompilerInputFormatMapping (only
  // "quake" and "catalyst" are), so this must fail at the input-format
  // validation step, before the compiler is ever invoked.
  auto task = makeTask("OPENQASM 3.0;", 2, "qasm3");
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::UnsupportedFormat);
}

TEST_F(CompileQuantumTaskTest, EmptyCircuitFailsInCompiler) {
  // Uses a supported circuit_file_type ("quake") so the empty circuit passes
  // input-format validation and is rejected by compileQuantumTask's own
  // empty-source guard. That guard exists because
  // MQSSCompiler::compileSource() segfaults on an empty source instead of
  // returning a diagnostic (mqss-ci's CommonMappingPass dereferences
  // kernel-analysis state that a module with no kernels never populates), so
  // this case must never reach the compiler.
  auto task = makeTask("" /* empty circuit */, 2, "quake");
  useCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::CompilationFailed);
  EXPECT_NE(result.error().detail.find("circuit file 0"), std::string::npos);
}

TEST_F(CompileQuantumTaskTest, InvalidCircuitSyntaxFailsInCompiler) {
  auto task = makeTask("THIS IS NOT VALID QUAKE MLIR SYNTAX;", 2, "quake");
  useCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::CompilationFailed);
  EXPECT_NE(result.error().detail.find("circuit file 0"), std::string::npos);
}

TEST_F(CompileQuantumTaskTest, FailureMessageIdentifiesFailingCircuitIndex) {
  // The first circuit file is valid, the second is not, so compileQuantumTask
  // must report index 1 -- not just "some" failure -- confirming the index
  // in the message tracks the actual failing file rather than being
  // hardcoded.
  auto task = makeTask(R"(
      module {
      func.func @hadamard_circuit() {
        %q0 = quake.alloca !quake.ref
        quake.h %q0 : (!quake.ref) -> ()
        %b0 = quake.mz %q0 : (!quake.ref) -> !quake.measure
        return
      }
    }
  )",
                       2, "quake");
  task.add_circuit_files("THIS IS NOT VALID QUAKE MLIR SYNTAX;");
  useCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::CompilationFailed);
  EXPECT_NE(result.error().detail.find("circuit file 1"), std::string::npos);
}

TEST_F(CompileQuantumTaskTest, NoModifyTaskSkipsCompilation) {
  auto task = makeTask("THIS IS NOT VALID QUAKE MLIR SYNTAX;", 2, "quake");
  task.set_no_modify(true);
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_TRUE(result.has_value());
}

// ===========================================================================
// compileQuantumTask -- per-circuit failure index among several circuits
// ===========================================================================
// FailureMessageIdentifiesFailingCircuitIndex above already proves index
// tracking at index 1 of 2; this extends the same technique to index 2 of 5
// so the reported index is confirmed to track the actual failing file rather
// than being hardcoded to the first or last position.

TEST_F(CompileQuantumTaskTest,
       FailureMessageIdentifiesFailingCircuitIndexAmongFive) {
  const std::string validQuake = R"(
      module {
      func.func @hadamard_circuit() {
        %q0 = quake.alloca !quake.ref
        quake.h %q0 : (!quake.ref) -> ()
        %b0 = quake.mz %q0 : (!quake.ref) -> !quake.measure
        return
      }
    }
  )";
  auto task = makeTask(validQuake, 2, "quake");
  for (int i = 1; i < 5; ++i) {
    task.add_circuit_files(i == 2 ? "THIS IS NOT VALID QUAKE MLIR SYNTAX;"
                                  : validQuake);
  }
  useCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});

  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::CompilationFailed);
  EXPECT_NE(result.error().detail.find("circuit file 2"), std::string::npos);
}

TEST_F(CompileQuantumTaskTest, InvalidOptimisationLevelFailsBeforeCompiler) {
  // An out-of-range optimisation_level must be rejected by
  // getCompilerOptimizationLevel() before the compiler ever runs, not
  // silently substituted with another level.
  auto task = makeTask(R"(
      module {
      func.func @hadamard_circuit() {
        %q0 = quake.alloca !quake.ref
        quake.h %q0 : (!quake.ref) -> ()
        %b0 = quake.mz %q0 : (!quake.ref) -> !quake.measure
        return
      }
    }
  )",
                       2, "quake");
  task.set_optimisation_level(4);
  useCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::CompilationFailed);
  EXPECT_NE(result.error().detail.find('4'), std::string::npos);
}

TEST_F(CompileQuantumTaskTest, NoCompatibleResultFormatFails) {
  // The default helper backend only supports CIRCUIT_FORMAT_QASM3, which the
  // compiler cannot emit, so BackendWrapper::compilerTarget() must fail
  // before the compiler is invoked.
  auto task = makeTask("OPENQASM 3.0;", 2, "quake");
  ASSERT_EQ(backend.getSupportedCircuitFormats().size(), 1u);
  ASSERT_EQ(backend.getSupportedCircuitFormats()[0],
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3);
  auto result = mqss::qrmci::compileQuantumTask(task, backend);
  EXPECT_FALSE(result.has_value());
  // The error is BackendWrapper::compilerTarget()'s, forwarded unchanged: no
  // backend-supported format can ever be emitted for this backend.
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::UnsupportedFormat);
}

} // namespace mqss::qrmci::test
