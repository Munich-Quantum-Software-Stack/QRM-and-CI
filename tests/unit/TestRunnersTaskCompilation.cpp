/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "CircuitFixtures.h"
#include "RunnersTestHelpers.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"
#include "qrmci/Error.h"
#include "qrmci/Runners.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <ostream>
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
  task.add_circuit_files(readCircuitFixture("bell.quake"));
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
  auto task = makeTask(readCircuitFixture("bell.quake"), 2, "quake");
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
  const std::string validQuake = readCircuitFixture("bell.quake");
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
  auto task = makeTask(readCircuitFixture("bell.quake"), 2, "quake");
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

// ===========================================================================
// compileQuantumTask -- every sample circuit against every output format
// ===========================================================================
// The cases above prove the routing and the failure paths; these prove the
// compiler actually emits the format the chosen target claims. A backend is
// built advertising exactly one circuit format, which pins down which entry
// of CompilerTargetPriority chooseCompilerTarget() lands on, and the compiled
// bytes are then checked for that format's own identifying markers.
//
// Markers only -- deliberately not a byte-for-byte comparison against stored
// output, which would break on every mqss-ci bump without distinguishing a
// regression from an improvement.

/// @brief One (sample circuit, output format) pair in the coverage matrix.
struct CompilationMatrixCase {
  std::string_view circuitFixture;
  mqss::CircuitFormat backendFormat;
  std::string_view expectedCircuitType;
  std::vector<std::string> expectedMarkers;
};

/// @brief Render a case in GoogleTest failure output.
/// @param testCase The case being reported.
/// @param stream Destination stream supplied by GoogleTest.
/// Without this GoogleTest falls back to a hex dump of the parameter's bytes,
/// which says nothing about which circuit or format failed.
void PrintTo(const CompilationMatrixCase &testCase, std::ostream *stream) {
  *stream << testCase.circuitFixture << " -> " << testCase.expectedCircuitType;
}

class CompileQuantumTaskMatrixTest
    : public ::testing::TestWithParam<CompilationMatrixCase> {};

TEST_P(CompileQuantumTaskMatrixTest, SampleCircuitCompilesToTargetFormat) {
  const auto &testCase = GetParam();
  auto task = makeTask(readCircuitFixture(testCase.circuitFixture), 3, "quake");
  const auto backend =
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {testCase.backendFormat});

  const auto result = mqss::qrmci::compileQuantumTask(task, backend);
  ASSERT_TRUE(result.has_value())
      << "compilation failed: " << result.error().detail;

  EXPECT_EQ(task.circuit_file_type(), testCase.expectedCircuitType);
  ASSERT_EQ(task.circuit_files_size(), 1);
  const std::string &compiled = task.circuit_files(0);

  for (const auto &marker : testCase.expectedMarkers) {
    EXPECT_NE(compiled.find(marker), std::string::npos)
        << "compiled output is missing the marker '" << marker
        << "' that identifies " << testCase.expectedCircuitType
        << ". Output was:\n"
        << compiled;
  }

  // Guards against a compiler that hands the source straight back: whatever
  // comes out, it is no longer Quake MLIR.
  EXPECT_EQ(compiled.find("quake."), std::string::npos)
      << "compiled output still contains Quake MLIR:\n"
      << compiled;
}

namespace {

/// @brief Identifying markers for one compiler output format.
struct TargetExpectation {
  mqss::CircuitFormat backendFormat;
  std::string_view taskCircuitType;
  std::vector<std::string> markers;
};

/// @brief Build the full (circuit x output format) matrix.
/// @return Every sample circuit paired with every format the compiler emits.
std::vector<CompilationMatrixCase> compilationMatrixCases() {
  // no-entrypoint.quake is excluded on purpose: it is not a CUDA-Q kernel and
  // compiles to a degenerate program, which
  // CircuitWithoutEntrypointCompilesToAProgramWithNoGates pins separately.
  constexpr std::array<std::string_view, 6> Circuits{
      "hadamard.quake",  "bell.quake",         "ghz.quake",
      "rotations.quake", "toggle-chain.quake", "some-circuit.quake"};

  const std::array<TargetExpectation, 3> targets{
      TargetExpectation{mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2,
                        "qasm2",
                        // "qreg" rather than the OPENQASM header alone: the
                        // header is emitted even for a program with no gates
                        // at all, so only a register declaration shows the
                        // circuit itself survived.
                        {"OPENQASM 2.0;", "qreg "}},
      TargetExpectation{mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING,
                        "qirbase",
                        {R"("qir_profiles"="base_profile")", R"("entry_point")",
                         "@__quantum__qis__"}},
      TargetExpectation{
          mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING,
          "qiradaptive",
          // Base and adaptive QIR differ only in this profile tag, so it is
          // the one marker that tells the two targets apart.
          {R"("qir_profiles"="adaptive_profile")", R"("entry_point")",
           "@__quantum__qis__"}},
  };

  std::vector<CompilationMatrixCase> cases;
  cases.reserve(Circuits.size() * targets.size());
  for (const auto &circuit : Circuits) {
    for (const auto &target : targets) {
      cases.push_back(CompilationMatrixCase{circuit, target.backendFormat,
                                            target.taskCircuitType,
                                            target.markers});
    }
  }
  return cases;
}

/// @brief Render a case as a GoogleTest-legal name, e.g. "bell_To_qirbase".
std::string
matrixCaseName(const ::testing::TestParamInfo<CompilationMatrixCase> &info) {
  std::string circuit{info.param.circuitFixture};
  circuit.erase(circuit.find(".quake"));
  std::ranges::replace(circuit, '-', '_');
  return circuit + "_To_" + std::string{info.param.expectedCircuitType};
}

} // namespace

INSTANTIATE_TEST_SUITE_P(SampleCircuits, CompileQuantumTaskMatrixTest,
                         ::testing::ValuesIn(compilationMatrixCases()),
                         matrixCaseName);

TEST_F(CompileQuantumTaskTest,
       CircuitWithoutEntrypointCompilesToAProgramWithNoGates) {
  // A Quake kernel missing the "cudaq-entrypoint"/"cudaq-kernel" attributes
  // is not rejected: MQSSCompiler reports success while emitting only an
  // OpenQASM 2 preamble, so compileQuantumTask hands back a circuit that has
  // lost every gate. Pinned here because nothing in the error path reveals
  // it -- if the compiler ever starts rejecting this input instead, this test
  // is the one that should be updated to match.
  auto task = makeTask(readCircuitFixture("no-entrypoint.quake"), 1, "quake");
  useCircuitFormats({mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2});

  const auto result = mqss::qrmci::compileQuantumTask(task, backend);
  ASSERT_TRUE(result.has_value())
      << "expected the compiler to still report success: "
      << (result ? std::string{} : result.error().detail);

  const std::string &compiled = task.circuit_files(0);
  EXPECT_NE(compiled.find("OPENQASM 2.0;"), std::string::npos);
  EXPECT_EQ(compiled.find("qreg "), std::string::npos)
      << "a register declaration means the gates survived after all, so this "
         "circuit is no longer degenerate:\n"
      << compiled;
}

} // namespace mqss::qrmci::test
