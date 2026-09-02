/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "RunnersTestHelpers.h"
#include "Submitter.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"
#include "qrmci/Error.h"
#include "qrmci/Runners.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mqss::qrmci::test {

// ===========================================================================
// executeQuantumTask (real Submitter, live QDMI driver)
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

  void SetUp() override {
    task = makeTask();
    task.set_task_id(42);
    task.set_result_destination("results-queue");
    task.set_scheduled_qpu("qdmi_example_driver");
  }
};

TEST_F(ExecuteQuantumTaskTest, ValidTaskWithValidSubmitter) {
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->execution_status(), true);
}

TEST_F(ExecuteQuantumTaskTest, ResultPreservesTaskId) {
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->task_id(), task.task_id());
}

TEST_F(ExecuteQuantumTaskTest, ResultPreservesDestination) {
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->destination(), task.result_destination());
}

TEST_F(ExecuteQuantumTaskTest, ResultPreservesExecutedQpu) {
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->executed_qpu(), task.scheduled_qpu());
}

TEST_F(ExecuteQuantumTaskTest, EmptyCircuitFilesWithValidSubmitter) {
  task.clear_circuit_files();
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::SubmissionFailed);
  EXPECT_FALSE(result.error().detail.empty());
}

TEST_F(ExecuteQuantumTaskTest, ZeroNumShotsWithValidSubmitter) {
  task.set_n_shots(0);
  auto result = mqss::qrmci::executeQuantumTask(task, submitter);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::SubmissionFailed);
  EXPECT_FALSE(result.error().detail.empty());
}

TEST_F(ExecuteQuantumTaskTest, MatchesSubmitThenCollectForTheSameTask) {
  // executeQuantumTask is defined as submitQuantumTask().and_then(
  // collectQuantumResult); pin the two paths together so they can't diverge.
  auto viaExecute = mqss::qrmci::executeQuantumTask(task, submitter);
  ASSERT_TRUE(viaExecute.has_value());

  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  ASSERT_TRUE(jobId.has_value());
  auto viaSubmitAndCollect =
      mqss::qrmci::collectQuantumResult(task, *jobId, submitter);
  ASSERT_TRUE(viaSubmitAndCollect.has_value());

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
// submitQuantumTask / collectQuantumResult (real Submitter, live QDMI driver)
// Confirms the two functions, composed, behave equivalently to
// executeQuantumTask -- this composition is exactly what the distributed
// worker's pipelined submit-then-collect flow relies on.
// ===========================================================================
class SubmitAndCollectQuantumTaskTest : public ::testing::Test {
protected:
  mqss::submitter::Submitter submitter = mqss::submitter::Submitter(
      "qdmi_example_driver", "C++ Device with 5 qubits",
      "C++ Device with 5 qubits", "example_token");

  mqss::QuantumTask task;

  void SetUp() override {
    task = makeTask();
    task.set_task_id(42);
    task.set_result_destination("results-queue");
    task.set_scheduled_qpu("qdmi_example_driver");
  }
};

TEST_F(SubmitAndCollectQuantumTaskTest, ValidTaskWithValidSubmitter) {
  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  ASSERT_TRUE(jobId.has_value());
  auto result = mqss::qrmci::collectQuantumResult(task, *jobId, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->execution_status(), true);
}

TEST_F(SubmitAndCollectQuantumTaskTest, ResultPreservesTaskId) {
  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  ASSERT_TRUE(jobId.has_value());
  auto result = mqss::qrmci::collectQuantumResult(task, *jobId, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->task_id(), task.task_id());
}

TEST_F(SubmitAndCollectQuantumTaskTest, ResultPreservesDestination) {
  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  ASSERT_TRUE(jobId.has_value());
  auto result = mqss::qrmci::collectQuantumResult(task, *jobId, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->destination(), task.result_destination());
}

TEST_F(SubmitAndCollectQuantumTaskTest, ResultPreservesExecutedQpu) {
  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  ASSERT_TRUE(jobId.has_value());
  auto result = mqss::qrmci::collectQuantumResult(task, *jobId, submitter);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->executed_qpu(), task.scheduled_qpu());
}

TEST_F(SubmitAndCollectQuantumTaskTest, EmptyCircuitFilesWithValidSubmitter) {
  task.clear_circuit_files();
  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  EXPECT_FALSE(jobId.has_value());
  EXPECT_EQ(jobId.error().kind, mqss::qrmci::Error::Kind::SubmissionFailed);
  // Transient: the device refusing a job now says nothing about the next
  // attempt, so the pipeline may requeue it.
  EXPECT_TRUE(jobId.error().isRetryable());
}

TEST_F(SubmitAndCollectQuantumTaskTest, ZeroNumShotsWithValidSubmitter) {
  task.set_n_shots(0);
  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  EXPECT_FALSE(jobId.has_value());
  EXPECT_EQ(jobId.error().kind, mqss::qrmci::Error::Kind::SubmissionFailed);
}

TEST_F(SubmitAndCollectQuantumTaskTest,
       PipelinedSubmissionOfMultipleTasksBeforeCollecting) {
  // The distributed worker submits every ready job first, then collects
  // results afterward (pipelined batch submission), instead of blocking on
  // each task's result before submitting the next. Two independent tasks
  // must each get their own job ID, and both must still be collectible
  // afterward.
  mqss::QuantumTask secondTask = makeTask();
  secondTask.set_task_id(43);
  secondTask.set_result_destination("results-queue");
  secondTask.set_scheduled_qpu("qdmi_example_driver");

  auto firstJobId = mqss::qrmci::submitQuantumTask(task, submitter);
  auto secondJobId = mqss::qrmci::submitQuantumTask(secondTask, submitter);
  ASSERT_TRUE(firstJobId.has_value());
  ASSERT_TRUE(secondJobId.has_value());
  EXPECT_NE(*firstJobId, *secondJobId);

  auto firstResult =
      mqss::qrmci::collectQuantumResult(task, *firstJobId, submitter);
  auto secondResult =
      mqss::qrmci::collectQuantumResult(secondTask, *secondJobId, submitter);
  ASSERT_TRUE(firstResult.has_value());
  ASSERT_TRUE(secondResult.has_value());
  EXPECT_EQ(firstResult->task_id(), task.task_id());
  EXPECT_EQ(secondResult->task_id(), secondTask.task_id());
}

TEST_F(SubmitAndCollectQuantumTaskTest, NoResultFoundForUnsubmittedJobId) {
  // No submission ever happened for this ID, so getJobResultHistogram() must
  // report nullopt -- this is the "never submitted" half of the comment on
  // collectQuantumResult's Internal error.
  auto result =
      mqss::qrmci::collectQuantumResult(task, /*jobId=*/99999U, submitter);
  ASSERT_FALSE(result.has_value());
  // A job ID with no result is an invariant violation, not something a retry
  // can fix.
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::Internal);
  EXPECT_FALSE(result.error().isRetryable());
  EXPECT_NE(result.error().detail.find("no result found for job"),
            std::string::npos);
}

TEST_F(SubmitAndCollectQuantumTaskTest,
       NoResultFoundOnSecondCollectionOfSameJobId) {
  // Submitter::getJobResultHistogram() erases a job from activeJobs once
  // retrieved, so collecting the same job ID a second time is the "already
  // retrieved" half of the same comment.
  auto jobId = mqss::qrmci::submitQuantumTask(task, submitter);
  ASSERT_TRUE(jobId.has_value());
  auto first = mqss::qrmci::collectQuantumResult(task, *jobId, submitter);
  ASSERT_TRUE(first.has_value());

  auto second = mqss::qrmci::collectQuantumResult(task, *jobId, submitter);
  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error().kind, mqss::qrmci::Error::Kind::Internal);
  EXPECT_FALSE(second.error().isRetryable());
  EXPECT_NE(second.error().detail.find("no result found for job"),
            std::string::npos);
}

// ===========================================================================
// buildQuantumResult (direct, no submitter)
// ===========================================================================
TEST(BuildQuantumResultTest, DeviceErrorWhenEveryHistogramEntryIsNullopt) {
  mqss::QuantumTask task = makeTask();
  task.set_task_id(42);
  task.set_result_destination("results-queue");
  task.set_scheduled_qpu("fake-device");

  const std::vector<std::optional<std::map<std::string, size_t>>> histogram{
      std::nullopt};
  auto result = mqss::qrmci::buildQuantumResult(task, histogram);
  ASSERT_FALSE(result.has_value());
  // The device took the job and returned nothing usable -- transient, so
  // this one may be retried.
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::DeviceError);
  EXPECT_TRUE(result.error().isRetryable());
  EXPECT_NE(result.error().detail.find("No valid results"), std::string::npos);
}

TEST(BuildQuantumResultTest, SingleCircuitWithCountsProducesOneResultEntry) {
  mqss::QuantumTask task = makeTask();
  task.set_task_id(7);
  task.set_result_destination("results-queue");
  task.set_scheduled_qpu("alpha");

  const std::vector<std::optional<std::map<std::string, size_t>>> histogram{
      std::map<std::string, size_t>{{"00", 6}, {"11", 4}}};
  auto result = mqss::qrmci::buildQuantumResult(task, histogram);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->execution_status());
  ASSERT_EQ(result->results_size(), 1);
  const auto &counts = result->results(0).counts();
  ASSERT_EQ(counts.size(), 2);
  EXPECT_EQ(counts.at("00"), 6);
  EXPECT_EQ(counts.at("11"), 4);
}

TEST(BuildQuantumResultTest,
     MixedHistogramOnlyEmitsEntriesForSucceededCircuits) {
  // One circuit produced counts, the other produced nothing; the overall
  // result must still be marked successful (any real result is enough), and
  // only the succeeding circuit's counts are carried through -- this pins
  // the current skip-on-nullopt behavior so a change to it (e.g. an
  // "index-alignment fix" that emits an empty entry for the nullopt one
  // instead) is a deliberate, visible decision rather than an accident.
  mqss::QuantumTask task = makeTask();
  task.set_task_id(8);

  const std::vector<std::optional<std::map<std::string, size_t>>> histogram{
      std::map<std::string, size_t>{{"0", 10}}, std::nullopt};
  auto result = mqss::qrmci::buildQuantumResult(task, histogram);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->execution_status());
  ASSERT_EQ(result->results_size(), 1);
  EXPECT_EQ(result->results(0).counts().at("0"), 10);
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
  auto result = mqss::qrmci::cancelQuantumTask(task, reason);
  EXPECT_EQ(result.execution_status(), false);
}

TEST_F(CancelQuantumTaskTest, ValidReasonResultContainsReason) {
  const std::string reason = "Timeout exceeded";
  auto result = mqss::qrmci::cancelQuantumTask(task, reason);
  EXPECT_NE(result.additional_information().find(reason), std::string::npos);
}

TEST_F(CancelQuantumTaskTest, EmptyReasonStillReturnsCancelledStatus) {
  auto result = mqss::qrmci::cancelQuantumTask(task, "");
  EXPECT_EQ(result.execution_status(), false);
}

TEST_F(CancelQuantumTaskTest, PreservesTaskId) {
  auto result = mqss::qrmci::cancelQuantumTask(task, "some reason");
  EXPECT_EQ(result.task_id(), task.task_id());
}

TEST_F(CancelQuantumTaskTest, PreservesDestination) {
  auto result = mqss::qrmci::cancelQuantumTask(task, "some reason");
  EXPECT_EQ(result.destination(), task.result_destination());
}

TEST_F(CancelQuantumTaskTest, PreservesExecutedQpu) {
  auto result = mqss::qrmci::cancelQuantumTask(task, "some reason");
  EXPECT_EQ(result.executed_qpu(), task.scheduled_qpu());
}

} // namespace mqss::qrmci::test
