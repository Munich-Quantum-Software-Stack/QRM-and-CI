/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Exercises the "no suitable backend" cancellation path end to end. Note
// that mqss::qrmci::chooseBackend() (src/Runners.cpp) falls back to *any*
// online, compatible backend when preferred_qpu doesn't match, so merely
// misnaming preferred_qpu isn't enough to force this path against a live
// deployment -- a task must be incompatible with every backend currently
// online. A circuit_file_type that mqss::qrmci::canPrepareTaskForBackend()
// (include/qrmci/CircuitFormatPolicy.h) accepts for no backend guarantees
// that deterministically, independent of which/how many backends are
// registered, so this test works unmodified against apps/standalone or
// apps/distributed.

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

class SubmitTaskUnsupportedBackendTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto loadedConfig = mqss::qrmci::loadConfig();
    ASSERT_TRUE(loadedConfig) << loadedConfig.error().detail;
    config = *loadedConfig;
  }

  mqss::qrmci::Config config;
};

TEST_F(SubmitTaskUnsupportedBackendTest, CancelsTaskWithNoSuitableBackend) {
  mqss::QuantumTask qtask;
  qtask.set_task_id(mqss::qrmci::test::uniqueTaskId());
  qtask.add_circuit_files(
      std::string("irrelevant: no backend supports this format"));
  qtask.set_circuit_file_type(std::string("unsupported-format-xyz"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(
      std::string("tester.tasks.queue.unsupported_backend"));

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
  EXPECT_FALSE(taskResult.execution_status())
      << "Expected the task to be cancelled for lacking a suitable backend, "
         "but it executed successfully.";

  const std::string expectedPrefix = "CANCELLED: No suitable backend found";
  EXPECT_TRUE(taskResult.additional_information().starts_with(expectedPrefix))
      << "Expected additional_information to start with '" << expectedPrefix
      << "', got: '" << taskResult.additional_information() << "'";
}

} // namespace

int main(int argc, char **argv) {
  std::vector<mqss::qrmci::test::DaemonProcess> daemons;
  daemons.emplace_back("qrmcid-standalone", QRMCI_STANDALONE_DAEMON_PATH,
                       QRMCI_DAEMON_LOG_DIR);
  return mqss::qrmci::test::runWithDaemons(argc, argv, std::move(daemons));
}
