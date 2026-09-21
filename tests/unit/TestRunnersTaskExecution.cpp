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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <mqss/submitter/Client.h>
#include <mqss/submitter/Device.h>
#include <mqss/submitter/Job.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mqss::qrmci::test {

namespace {

// QRMCI_TEST_QDMI_DRIVER_PATH is set in tests/unit/CMakeLists.txt to the built
// example driver's own file. Client::openDevice() existence-checks the path, so
// a bare library stem is not enough. QDMI_CONF and LD_LIBRARY_PATH are injected
// by the same file's set_tests_properties(), which is why these tests must be
// run through ctest rather than by invoking the binary directly.
constexpr const char *kDriverPath = QRMCI_TEST_QDMI_DRIVER_PATH;
constexpr const char *kDeviceName = "C++ Device with 5 qubits";

/// @brief Waiting indefinitely, the same as the shipped default.
constexpr std::chrono::seconds kWaitForever{0};

/// @brief Opens the real example QDMI device shared by the fixtures below.
class DeviceFixture : public ::testing::Test {
protected:
  mqss::submitter::Client client;
  std::optional<mqss::submitter::Device> opened;
  mqss::QuantumTask task;

  void SetUp() override {
    auto result = client.openDevice(
        std::filesystem::path{kDriverPath}, kDeviceName,
        mqss::submitter::SessionConfig{.token = "example_token"},
        mqss::submitter::DeviceConfig{});
    ASSERT_TRUE(result.has_value()) << result.error().message();
    opened.emplace(std::move(*result));

    // "qasm2", not makeTask()'s own default "qasm3": the real example driver
    // this fixture opens only ever advertises {QASM2, QIRBASESTRING,
    // QIRBASEMODULE, CALIBRATION} as
    // QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS
    // (examples/device/src/cxx_device.cpp in every vendored QDMI version,
    // 1.3.0-1.3.3) -- QASM3 was never one of them, so a task left at the
    // helper's own default fails at the device with QDMI_ERROR_NOTSUPPORTED
    // before it ever gets to execute anything.
    task = makeTask("OPENQASM 2.0;", 2, "qasm2");
    task.set_task_id(42);
    task.set_result_destination("results-queue");
    task.set_scheduled_qpu("qdmi_example_driver");
  }

  mqss::submitter::Device &device() { return *opened; }
};

} // namespace

// ===========================================================================
// executeQuantumTask (real Device, live QDMI driver)
// ===========================================================================
using ExecuteQuantumTaskTest = DeviceFixture;

TEST_F(ExecuteQuantumTaskTest, ValidTaskWithValidDevice) {
  auto result = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->execution_status(), true);
}

TEST_F(ExecuteQuantumTaskTest, ResultPreservesTaskId) {
  auto result = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->task_id(), task.task_id());
}

TEST_F(ExecuteQuantumTaskTest, ResultPreservesDestination) {
  auto result = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->destination(), task.result_destination());
}

TEST_F(ExecuteQuantumTaskTest, ResultPreservesExecutedQpu) {
  auto result = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->executed_qpu(), task.scheduled_qpu());
}

TEST_F(ExecuteQuantumTaskTest, EmptyCircuitFilesIsASubmissionFailure) {
  task.clear_circuit_files();
  auto result = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::SubmissionFailed);
  EXPECT_FALSE(result.error().detail.empty());
}

TEST_F(ExecuteQuantumTaskTest, ZeroNumShotsIsASubmissionFailure) {
  task.set_n_shots(0);
  auto result = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::SubmissionFailed);
  EXPECT_FALSE(result.error().detail.empty());
}

TEST_F(ExecuteQuantumTaskTest, MatchesSubmitThenCollectForTheSameTask) {
  // executeQuantumTask is defined as submitQuantumTask().and_then(
  // collectQuantumResult); pin the two paths together so they can't diverge.
  auto viaExecute = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_TRUE(viaExecute.has_value()) << viaExecute.error().detail;

  auto jobs = submitQuantumTask(task, device());
  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  auto viaSubmitAndCollect = collectQuantumResult(task, *jobs, kWaitForever);
  ASSERT_TRUE(viaSubmitAndCollect.has_value())
      << viaSubmitAndCollect.error().detail;

  EXPECT_EQ(viaExecute->task_id(), viaSubmitAndCollect->task_id());
  EXPECT_EQ(viaExecute->destination(), viaSubmitAndCollect->destination());
  EXPECT_EQ(viaExecute->executed_qpu(), viaSubmitAndCollect->executed_qpu());
  EXPECT_EQ(viaExecute->execution_status(),
            viaSubmitAndCollect->execution_status());
  EXPECT_EQ(viaExecute->additional_information(),
            viaSubmitAndCollect->additional_information());
  EXPECT_EQ(viaExecute->results_size(), viaSubmitAndCollect->results_size());
}

// ===========================================================================
// submitQuantumTask / collectQuantumResult (real Device, live QDMI driver)
// The composition the distributed worker's pipelined submit-then-collect flow
// relies on.
// ===========================================================================
using SubmitAndCollectQuantumTaskTest = DeviceFixture;

TEST_F(SubmitAndCollectQuantumTaskTest, ValidTaskWithValidDevice) {
  auto jobs = submitQuantumTask(task, device());
  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  auto result = collectQuantumResult(task, *jobs, kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->execution_status(), true);
}

TEST_F(SubmitAndCollectQuantumTaskTest, OneJobPerCircuitFile) {
  // A Job carries exactly one payload, so a task with two circuit files
  // becomes two Jobs -- in the files' own order.
  task.add_circuit_files("OPENQASM 3.0;");
  ASSERT_EQ(task.circuit_files().size(), 2);

  auto jobs = submitQuantumTask(task, device());
  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  EXPECT_EQ(jobs->size(), 2U);
}

TEST_F(SubmitAndCollectQuantumTaskTest, ResultPreservesTaskId) {
  auto jobs = submitQuantumTask(task, device());
  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  auto result = collectQuantumResult(task, *jobs, kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->task_id(), task.task_id());
}

TEST_F(SubmitAndCollectQuantumTaskTest, ResultPreservesDestination) {
  auto jobs = submitQuantumTask(task, device());
  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  auto result = collectQuantumResult(task, *jobs, kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->destination(), task.result_destination());
}

TEST_F(SubmitAndCollectQuantumTaskTest, ResultPreservesExecutedQpu) {
  auto jobs = submitQuantumTask(task, device());
  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  auto result = collectQuantumResult(task, *jobs, kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->executed_qpu(), task.scheduled_qpu());
}

TEST_F(SubmitAndCollectQuantumTaskTest,
       EmptyCircuitFilesIsRefusedBeforeTheDevice) {
  task.clear_circuit_files();
  auto jobs = submitQuantumTask(task, device());
  ASSERT_FALSE(jobs.has_value());
  EXPECT_EQ(jobs.error().kind, Error::Kind::SubmissionFailed);
}

TEST_F(SubmitAndCollectQuantumTaskTest, ZeroNumShotsIsRefusedBeforeTheDevice) {
  task.set_n_shots(0);
  auto jobs = submitQuantumTask(task, device());
  ASSERT_FALSE(jobs.has_value());
  EXPECT_EQ(jobs.error().kind, Error::Kind::SubmissionFailed);
}

TEST_F(SubmitAndCollectQuantumTaskTest,
       PipelinedSubmissionOfMultipleTasksBeforeCollecting) {
  // The distributed worker submits every ready job first, then collects
  // results afterward (pipelined batch submission), instead of blocking on
  // each task's result before submitting the next. Two independent tasks must
  // each get their own jobs, and both must still be collectible afterward.
  mqss::QuantumTask secondTask = makeTask("OPENQASM 2.0;", 2, "qasm2");
  secondTask.set_task_id(43);
  secondTask.set_result_destination("results-queue");
  secondTask.set_scheduled_qpu("qdmi_example_driver");

  auto firstJobs = submitQuantumTask(task, device());
  auto secondJobs = submitQuantumTask(secondTask, device());
  ASSERT_TRUE(firstJobs.has_value()) << firstJobs.error().detail;
  ASSERT_TRUE(secondJobs.has_value()) << secondJobs.error().detail;

  auto firstResult = collectQuantumResult(task, *firstJobs, kWaitForever);
  auto secondResult =
      collectQuantumResult(secondTask, *secondJobs, kWaitForever);
  ASSERT_TRUE(firstResult.has_value()) << firstResult.error().detail;
  ASSERT_TRUE(secondResult.has_value()) << secondResult.error().detail;
  EXPECT_EQ(firstResult->task_id(), task.task_id());
  EXPECT_EQ(secondResult->task_id(), secondTask.task_id());
}

TEST_F(SubmitAndCollectQuantumTaskTest, NoJobsIsADeviceError) {
  // Nothing was submitted, so the histogram is empty while the task still
  // names a circuit file. buildQuantumResult's cardinality-mismatch check
  // covers it.
  std::vector<mqss::submitter::Job> none;
  auto result = collectQuantumResult(task, none, kWaitForever);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::DeviceError);
}

// ===========================================================================
// Interruptible overloads (real Device, live QDMI driver): the flag is
// consulted before any device work, so a pre-set flag proves the shutdown
// path is taken instead of racing a real device/job to prove it mid-flight --
// the same "pre-set flag" style TestCommunicationHandler.cpp's own
// termination-flag tests use for the same reason.
// ===========================================================================
using InterruptibleRunnersTest = DeviceFixture;

TEST_F(InterruptibleRunnersTest,
       SubmitQuantumTaskReturnsShutdownRequestedWhenFlagIsPreSet) {
  const std::atomic<bool> terminationFlag{true};
  auto jobs = submitQuantumTask(task, device(), terminationFlag);
  ASSERT_FALSE(jobs.has_value());
  EXPECT_EQ(jobs.error().kind, Error::Kind::ShutdownRequested);
}

TEST_F(InterruptibleRunnersTest,
       CollectQuantumResultReturnsShutdownRequestedWhenFlagIsPreSet) {
  auto jobs = submitQuantumTask(task, device());
  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;

  const std::atomic<bool> terminationFlag{true};
  auto result =
      collectQuantumResult(task, *jobs, kWaitForever, terminationFlag);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::ShutdownRequested);
}

TEST_F(InterruptibleRunnersTest,
       ExecuteQuantumTaskReturnsShutdownRequestedWhenFlagIsPreSet) {
  const std::atomic<bool> terminationFlag{true};
  auto result =
      executeQuantumTask(task, device(), kWaitForever, terminationFlag);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::ShutdownRequested);
}

TEST_F(InterruptibleRunnersTest,
       ExecuteQuantumTaskWithAnUnsetFlagStillSucceeds) {
  // Pins the interruptible overload's happy path to the same behavior as the
  // uninterruptible one, since the latter is now implemented in terms of it.
  const std::atomic<bool> terminationFlag{false};
  auto result =
      executeQuantumTask(task, device(), kWaitForever, terminationFlag);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_TRUE(result->execution_status());
}

// ===========================================================================
// executeQuantumTask (real Device) for every circuit type
// mapTaskCircuitTypeToCircuitFormat resolves to a CircuitFormat the real
// example driver actually advertises as supported -- confirms the mapping's
// translation reaches the device successfully rather than merely
// type-checking against ConstantsMapping.cpp's own table in isolation.
// ===========================================================================
class SupportedCircuitTypeTest
    : public DeviceFixture,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(SupportedCircuitTypeTest, SubmitsAndExecutesSuccessfully) {
  task = makeTask("OPENQASM 2.0;", 2, GetParam());
  task.set_task_id(42);
  task.set_result_destination("results-queue");
  task.set_scheduled_qpu("qdmi_example_driver");

  auto result = executeQuantumTask(task, device(), kWaitForever);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_TRUE(result->execution_status());
}

INSTANTIATE_TEST_SUITE_P(
    CircuitTypes, SupportedCircuitTypeTest,
    // "qasm2" maps directly to CIRCUIT_FORMAT_QASM2 (device-supported); "qir"
    // maps to the abstract CIRCUIT_FORMAT_QIR marker, which
    // mapTaskCircuitTypeToCircuitFormat resolves to the concrete
    // CIRCUIT_FORMAT_QIRBASESTRING (also device-supported) rather than
    // forwarding the marker itself.
    ::testing::Values("qasm2", "qir"),
    [](const ::testing::TestParamInfo<std::string> &info) {
      return info.param;
    });

// ===========================================================================
// buildQuantumResult (direct, no device)
// ===========================================================================
TEST(BuildQuantumResultTest, DeviceErrorWhenTheOnlyHistogramEntryIsNullopt) {
  mqss::QuantumTask task = makeTask();
  task.set_task_id(42);
  task.set_result_destination("results-queue");
  task.set_scheduled_qpu("fake-device");

  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      std::nullopt};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_FALSE(result.has_value());
  // The device took the job and returned nothing usable.
  EXPECT_EQ(result.error().kind, Error::Kind::DeviceError);
  EXPECT_NE(result.error().detail.find("circuit 0"), std::string::npos);
}

TEST(BuildQuantumResultTest, SingleCircuitWithCountsProducesOneResultEntry) {
  mqss::QuantumTask task = makeTask();
  task.set_task_id(7);
  task.set_result_destination("results-queue");
  task.set_scheduled_qpu("alpha");

  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      mqss::submitter::Counts{{"00", 6}, {"11", 4}}};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_TRUE(result->execution_status());
  ASSERT_EQ(result->results_size(), 1);
  const auto &counts = result->results(0).counts();
  ASSERT_EQ(counts.size(), 2);
  EXPECT_EQ(counts.at("00"), 6);
  EXPECT_EQ(counts.at("11"), 4);
}

TEST(BuildQuantumResultTest, MixedHistogramFailsWithTheMissingCircuitIndex) {
  // One circuit produced counts, the other produced nothing. Cardinality must
  // be preserved rather than silently collapsed, so a missing entry is a
  // DeviceError naming the circuit it belongs to, not a partial success.
  mqss::QuantumTask task = makeTask();
  task.add_circuit_files("OPENQASM 3.0;");
  task.set_task_id(8);

  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      mqss::submitter::Counts{{"0", 10}}, std::nullopt};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::DeviceError);
  EXPECT_NE(result.error().detail.find("circuit 1"), std::string::npos);
}

TEST(BuildQuantumResultTest, ShorterHistogramThanCircuitCountFails) {
  mqss::QuantumTask task = makeTask();
  task.add_circuit_files("OPENQASM 3.0;");
  ASSERT_EQ(task.circuit_files_size(), 2);

  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      mqss::submitter::Counts{{"0", 10}}};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::DeviceError);
}

TEST(BuildQuantumResultTest, LongerHistogramThanCircuitCountFails) {
  mqss::QuantumTask task = makeTask();
  ASSERT_EQ(task.circuit_files_size(), 1);

  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      mqss::submitter::Counts{{"0", 10}}, mqss::submitter::Counts{{"1", 5}}};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::DeviceError);
}

TEST(BuildQuantumResultTest,
     ExactCardinalityWithAllCountsProducesOneEntryPerCircuit) {
  mqss::QuantumTask task = makeTask();
  task.add_circuit_files("OPENQASM 3.0;");
  task.set_task_id(9);

  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      mqss::submitter::Counts{{"0", 10}}, mqss::submitter::Counts{{"1", 5}}};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  ASSERT_EQ(result->results_size(), 2);
  EXPECT_EQ(result->results(0).counts().at("0"), 10);
  EXPECT_EQ(result->results(1).counts().at("1"), 5);
}

TEST(BuildQuantumResultTest, CountAtInt32MaxSucceeds) {
  mqss::QuantumTask task = makeTask();

  const std::uint64_t maxInt32Count =
      static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      mqss::submitter::Counts{{"0", maxInt32Count}}};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(result->results(0).counts().at("0"),
            std::numeric_limits<std::int32_t>::max());
}

TEST(BuildQuantumResultTest,
     CountAboveInt32MaxFailsWithTheBitstringAndValueInDetail) {
  mqss::QuantumTask task = makeTask();

  const std::uint64_t overflowingCount =
      static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const std::vector<std::optional<mqss::submitter::Counts>> histogram{
      mqss::submitter::Counts{{"1010", overflowingCount}}};
  auto result = buildQuantumResult(task, histogram);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, Error::Kind::DeviceError);
  EXPECT_NE(result.error().detail.find("1010"), std::string::npos);
  EXPECT_NE(result.error().detail.find(std::to_string(overflowingCount)),
            std::string::npos);
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
    task.set_result_destination("results-queue");
    task.set_scheduled_qpu("alpha");
  }
};

TEST_F(CancelQuantumTaskTest, ValidReasonReturnsCancelledResult) {
  const std::string reason = "User requested cancellation";
  auto result = cancelQuantumTask(task, reason);
  EXPECT_EQ(result.execution_status(), false);
}

TEST_F(CancelQuantumTaskTest, ValidReasonResultContainsReason) {
  const std::string reason = "Timeout exceeded";
  auto result = cancelQuantumTask(task, reason);
  EXPECT_NE(result.additional_information().find(reason), std::string::npos);
}

TEST_F(CancelQuantumTaskTest, EmptyReasonStillReturnsCancelledStatus) {
  auto result = cancelQuantumTask(task, "");
  EXPECT_EQ(result.execution_status(), false);
}

TEST_F(CancelQuantumTaskTest, PreservesTaskId) {
  auto result = cancelQuantumTask(task, "some reason");
  EXPECT_EQ(result.task_id(), task.task_id());
}

TEST_F(CancelQuantumTaskTest, PreservesDestination) {
  auto result = cancelQuantumTask(task, "some reason");
  EXPECT_EQ(result.destination(), task.result_destination());
}

TEST_F(CancelQuantumTaskTest, PreservesExecutedQpu) {
  auto result = cancelQuantumTask(task, "some reason");
  EXPECT_EQ(result.executed_qpu(), task.scheduled_qpu());
}

} // namespace mqss::qrmci::test
