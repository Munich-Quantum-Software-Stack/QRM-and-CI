/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Exercises the apps/distributed pipeline (selector -> worker) rather than
// apps/standalone. The client-visible contract is identical to
// TestSubmitTask.cpp -- tasks go in on config.common.qrmciQueue and results
// come back on the caller-supplied result_destination -- because the
// distributed selector shares that same front-door queue with the
// standalone daemon; the only difference is which deployment is running:
// qrmcid-distributed-selector and qrmcid-distributed-worker, spawned by this
// binary's own DaemonEnvironment, rather than qrmcid-standalone.

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

class SubmitTaskDistributedTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto loadedConfig = mqss::qrmci::loadConfig();
    ASSERT_TRUE(loadedConfig) << loadedConfig.error().detail;
    config = *loadedConfig;
  }

  mqss::qrmci::Config config;
};

TEST_F(SubmitTaskDistributedTest, ReturnsResultForSubmittedTask) {
  mqss::QuantumTask qtask;
  qtask.set_task_id(mqss::qrmci::test::uniqueTaskId());
  qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  // The client-facing backend ID (registry key, and what preferred_qpu/
  // scheduled_qpu/executed_qpu actually match against), not the QDMI
  // device's own display name -- see openConfiguredDevice()'s deviceId()
  // documentation.
  qtask.set_preferred_qpu(std::string("cxxdevice5q"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(std::string("tester.tasks.queue.distributed"));

  mqss::qrmci::CommunicationHandler communicationHandler(
      config.common.connection);
  auto sent = communicationHandler.send(qtask, config.common.qrmciQueue);
  ASSERT_TRUE(sent) << sent.error().detail;

  auto received = communicationHandler.receive<mqss::QuantumResult>(
      qtask.result_destination(), mqss::qrmci::test::kResultTimeout);
  ASSERT_TRUE(received) << received.error().detail;
  ASSERT_TRUE(received->has_value()) << "No result received from the queue.";

  const mqss::QuantumResult &taskResult = received->value();
  ASSERT_EQ(taskResult.task_id(), qtask.task_id());
  EXPECT_TRUE(taskResult.execution_status())
      << "Task did not execute successfully: "
      << taskResult.additional_information();
}

} // namespace

int main(int argc, char **argv) {
  std::vector<mqss::qrmci::test::DaemonProcess> daemons;
  daemons.emplace_back("qrmcid-distributed-selector",
                       QRMCI_DISTRIBUTED_SELECTOR_PATH,
                       QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR);
  daemons.emplace_back("qrmcid-distributed-worker",
                       QRMCI_DISTRIBUTED_WORKER_PATH,
                       QRMCI_DISTRIBUTED_WORKER_LOG_DIR);
  // The worker publishes its own backend status to the selector on
  // SubmitterConfig::backendStatusPublishInterval (5s by default); a task
  // submitted before the first publish lands finds an empty registry and is
  // legitimately cancelled for lacking a suitable backend, not delayed --
  // see DaemonEnvironment's own documentation for why the broker-readiness
  // probe alone cannot detect this.
  return mqss::qrmci::test::runWithDaemons(argc, argv, std::move(daemons),
                                           std::chrono::seconds(6));
}
