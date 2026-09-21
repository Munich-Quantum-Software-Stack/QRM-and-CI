/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/Protocol.hpp"
#include "qrmci/CircuitFormatPolicy.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <array>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mqss::qrmci::test {

namespace {

mqss::QuantumTask makeTask(std::string_view circuitFileType, bool noModify) {
  mqss::QuantumTask task;
  task.set_circuit_file_type(std::string(circuitFileType));
  task.set_no_modify(noModify);
  return task;
}

} // namespace

// ===========================================================================
// mapTaskCircuitTypeToCircuitFormat
// An exhaustive, deterministic switch: every task circuit type resolves to
// exactly one circuit format, independent of any lookup-table iteration
// order.
// ===========================================================================
struct DirectMappingCase {
  std::string_view taskCircuitType;
  mqss::CircuitFormat expected;
};

class MapTaskCircuitTypeToCircuitFormatTest
    : public ::testing::TestWithParam<DirectMappingCase> {};

TEST_P(MapTaskCircuitTypeToCircuitFormatTest, MapsToTheExpectedFormat) {
  EXPECT_EQ(mapTaskCircuitTypeToCircuitFormat(
                std::string(GetParam().taskCircuitType)),
            GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
    EveryKnownTaskCircuitType, MapTaskCircuitTypeToCircuitFormatTest,
    ::testing::Values(
        DirectMappingCase{"qasm", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2},
        DirectMappingCase{"qasm2", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2},
        DirectMappingCase{"qasm3", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3},
        DirectMappingCase{"qir",
                          mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        DirectMappingCase{"qirbase",
                          mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        DirectMappingCase{
            "qiradaptive",
            mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING},
        // Quake/Catalyst have no distinct protocol wire-format enumerator;
        // direct submission uses QIRBASESTRING.
        DirectMappingCase{"quake",
                          mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        DirectMappingCase{"catalyst",
                          mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        DirectMappingCase{"unknowntype",
                          mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED},
        DirectMappingCase{"", mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED}),
    [](const ::testing::TestParamInfo<DirectMappingCase> &info) {
      return info.param.taskCircuitType.empty()
                 ? "Empty"
                 : std::string(info.param.taskCircuitType);
    });

// ===========================================================================
// chooseCompilerTarget
// ===========================================================================
TEST(ChooseCompilerTargetTest, Qasm2WinsWhenPresent) {
  const std::array formats = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING};
  auto target = chooseCompilerTarget(formats);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->compilerFormat, mqss::mqssci::ResultFormat::OPENQASM2);
  EXPECT_EQ(target->circuitFormat, mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2);
  EXPECT_EQ(target->taskCircuitType, "qasm2");
}

TEST(ChooseCompilerTargetTest, PriorityIsFixedNotBackendListOrder) {
  // Same two formats, opposite list order: the priority table -- not the
  // backend's own order -- decides the winner.
  const std::array forward = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING};
  const std::array reversed = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2};
  auto forwardTarget = chooseCompilerTarget(forward);
  auto reversedTarget = chooseCompilerTarget(reversed);
  ASSERT_TRUE(forwardTarget.has_value());
  ASSERT_TRUE(reversedTarget.has_value());
  EXPECT_EQ(forwardTarget->circuitFormat, reversedTarget->circuitFormat);
  EXPECT_EQ(forwardTarget->circuitFormat,
            mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2);
}

TEST(ChooseCompilerTargetTest, QirbasestringWinsWhenQasm2Absent) {
  const std::array formats = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING};
  auto target = chooseCompilerTarget(formats);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->compilerFormat, mqss::mqssci::ResultFormat::QIRBASE);
  EXPECT_EQ(target->taskCircuitType, "qirbase");
}

TEST(ChooseCompilerTargetTest, QiradaptivestringWinsWhenNothingHigherPresent) {
  const std::array formats = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING};
  auto target = chooseCompilerTarget(formats);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->compilerFormat, mqss::mqssci::ResultFormat::QIRADAPTIVE);
  EXPECT_EQ(target->taskCircuitType, "qiradaptive");
}

TEST(ChooseCompilerTargetTest, ModuleVariantsAreNeverSelected) {
  // MQSS Compiler only ever returns textual bytes, so a backend offering
  // only module variants (or the abstract QIR marker) has no compiler
  // target, even though those formats do exist in the protocol.
  const std::array formats = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIR,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE};
  auto target = chooseCompilerTarget(formats);
  EXPECT_FALSE(target.has_value());
}

TEST(ChooseCompilerTargetTest, NoCompatibleFormatFails) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  auto target = chooseCompilerTarget(formats);
  ASSERT_FALSE(target.has_value());
  EXPECT_EQ(target.error().kind, mqss::qrmci::Error::Kind::UnsupportedFormat);
}

TEST(ChooseCompilerTargetTest, EmptyFormatListFails) {
  EXPECT_FALSE(chooseCompilerTarget({}).has_value());
}

// ===========================================================================
// canPrepareTaskForBackend
// The admission policy matrix: no_modify=true takes the direct-submission
// path (mapTaskCircuitTypeToCircuitFormat); no_modify=false takes the
// compiler-input-plus-compilerTarget path. A task with no_modify=false and a
// non-compiler input is rejected here, at admission, never later at
// compilation.
// ===========================================================================
class CanPrepareTaskForBackendNoModifyTest : public ::testing::Test {};

TEST_F(CanPrepareTaskForBackendNoModifyTest, DirectFormatPresentSucceeds) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  EXPECT_TRUE(canPrepareTaskForBackend(makeTask("qasm3", true), formats));
}

TEST_F(CanPrepareTaskForBackendNoModifyTest, DirectFormatAbsentFails) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2};
  EXPECT_FALSE(canPrepareTaskForBackend(makeTask("qasm3", true), formats));
}

TEST_F(CanPrepareTaskForBackendNoModifyTest, UnknownCircuitTypeFails) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  EXPECT_FALSE(
      canPrepareTaskForBackend(makeTask("no-such-type", true), formats));
}

TEST_F(CanPrepareTaskForBackendNoModifyTest, EmptyFormatListFails) {
  EXPECT_FALSE(canPrepareTaskForBackend(makeTask("qasm3", true), {}));
}

class CanPrepareTaskForBackendCompilableTest : public ::testing::Test {};

TEST_F(CanPrepareTaskForBackendCompilableTest,
       CompilerInputWithCompilerTargetSucceeds) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2};
  EXPECT_TRUE(canPrepareTaskForBackend(makeTask("quake", false), formats));
  EXPECT_TRUE(canPrepareTaskForBackend(makeTask("catalyst", false), formats));
}

TEST_F(CanPrepareTaskForBackendCompilableTest,
       CompilerInputWithoutCompilerTargetFails) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3};
  EXPECT_FALSE(canPrepareTaskForBackend(makeTask("quake", false), formats));
}

TEST_F(CanPrepareTaskForBackendCompilableTest,
       NonCompilerInputFailsRegardlessOfBackendFormats) {
  // A task with no_modify=false and a non-compiler input must be rejected
  // during admission, even if the backend directly supports that circuit
  // format. Only quake and catalyst are valid compiler inputs.
  const std::array everyPriorityFormat = {
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING,
      mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING};
  for (const std::string_view type :
       {"qasm", "qasm2", "qasm3", "qir", "qirbase", "qiradaptive"}) {
    EXPECT_FALSE(
        canPrepareTaskForBackend(makeTask(type, false), everyPriorityFormat))
        << "type: " << type;
  }
}

TEST_F(CanPrepareTaskForBackendCompilableTest, UnknownCircuitTypeFails) {
  const std::array formats = {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2};
  EXPECT_FALSE(
      canPrepareTaskForBackend(makeTask("no-such-type", false), formats));
}

// ===========================================================================
// validateInputFormatIsSupportedCircuitFormat
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
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::UnsupportedFormat);
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
