/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Exercises routing across two distributed workers rather than one: each
// worker advertises a distinct backend ID and dispatch queue
// (QRMCI_SUBMITTER_QDMI_DEVICE_ID / QRMCI_COMPILER_QUEUE), so the selector
// must send a task it assigns to worker-b to worker-b's own queue rather
// than a queue every worker shares.
// Each task is restricted (QuantumTask::restricted_resource_names) to the
// one worker ID it must land on, and the assertion that actually catches a
// wrong-worker delivery is on the result's executed_qpu, not merely on
// execution_status: a task the wrong worker picked up and compiled for
// itself would still come back successful, just with the wrong
// executed_qpu.

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

mqss::QuantumTask makeTaskRestrictedTo(const std::string &workerId) {
  mqss::QuantumTask qtask;
  qtask.set_task_id(mqss::qrmci::test::uniqueTaskId());
  qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.add_restricted_resource_names(workerId);
  qtask.set_no_modify(false);
  qtask.set_result_destination("tester.tasks.queue.multiworker." + workerId);
  return qtask;
}

class SubmitTaskMultiWorkerTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto loadedConfig = mqss::qrmci::loadConfig();
    ASSERT_TRUE(loadedConfig) << loadedConfig.error().detail;
    config = *loadedConfig;
  }

  mqss::qrmci::Config config;
};

TEST_F(SubmitTaskMultiWorkerTest, RoutesEachRestrictedTaskToItsOwnWorker) {
  const std::vector<std::string> workerIds = {"worker-a", "worker-b"};

  std::vector<mqss::QuantumTask> tasks;
  tasks.reserve(workerIds.size());
  for (const auto &workerId : workerIds) {
    tasks.push_back(makeTaskRestrictedTo(workerId));
  }

  mqss::qrmci::CommunicationHandler communicationHandler(
      config.common.connection);

  for (const auto &qtask : tasks) {
    auto sent = communicationHandler.send(qtask, config.common.qrmciQueue);
    ASSERT_TRUE(sent) << "Sending task_id=" << qtask.task_id() << ": "
                      << sent.error().detail;
  }

  for (std::size_t i = 0; i < tasks.size(); ++i) {
    const auto &qtask = tasks[i];
    SCOPED_TRACE("expected worker=" + workerIds[i] +
                 " task_id=" + std::to_string(qtask.task_id()));

    auto received = communicationHandler.receive<mqss::QuantumResult>(
        qtask.result_destination(), mqss::qrmci::test::kResultTimeout);
    ASSERT_TRUE(received) << received.error().detail;
    ASSERT_TRUE(received->has_value())
        << "No result received on queue " << qtask.result_destination();

    const mqss::QuantumResult &taskResult = received->value();
    EXPECT_EQ(taskResult.task_id(), qtask.task_id());
    EXPECT_TRUE(taskResult.execution_status())
        << "Task did not execute successfully: "
        << taskResult.additional_information();
    EXPECT_EQ(taskResult.executed_qpu(), workerIds[i])
        << "Task was executed by the wrong worker.";
  }
}

} // namespace

int main(int argc, char **argv) {
  std::vector<mqss::qrmci::test::DaemonProcess> daemons;
  daemons.emplace_back("qrmcid-distributed-selector",
                       QRMCI_DISTRIBUTED_SELECTOR_PATH,
                       QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR);
  daemons.emplace_back("qrmcid-distributed-worker-a",
                       QRMCI_DISTRIBUTED_WORKER_PATH,
                       QRMCI_DISTRIBUTED_WORKER_A_LOG_DIR,
                       std::vector<mqss::qrmci::test::EnvOverride>{
                           {"QRMCI_SUBMITTER_QDMI_DEVICE_ID", "worker-a"},
                           {"QRMCI_COMPILER_QUEUE", "compiler.worker-a"},
                       });
  daemons.emplace_back("qrmcid-distributed-worker-b",
                       QRMCI_DISTRIBUTED_WORKER_PATH,
                       QRMCI_DISTRIBUTED_WORKER_B_LOG_DIR,
                       std::vector<mqss::qrmci::test::EnvOverride>{
                           {"QRMCI_SUBMITTER_QDMI_DEVICE_ID", "worker-b"},
                           {"QRMCI_COMPILER_QUEUE", "compiler.worker-b"},
                       });
  // Two workers each publish their own status on the same
  // backendStatusPublishInterval as a single-worker deployment, so the same
  // extra readiness grace TestSubmitTaskDistributed.cpp uses applies here.
  return mqss::qrmci::test::runWithDaemons(argc, argv, std::move(daemons),
                                           std::chrono::seconds(6));
}
