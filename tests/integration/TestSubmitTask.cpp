/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

class SubmitTaskTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto loadedConfig = mqss::qrmci::loadConfig();
    ASSERT_TRUE(loadedConfig) << loadedConfig.error().detail;
    config = *loadedConfig;
  }

  mqss::qrmci::Config config;
};

TEST_F(SubmitTaskTest, ReturnsResultForSubmittedTask) {
  mqss::QuantumTask qtask;
  const auto taskId = mqss::qrmci::test::uniqueTaskId();
  qtask.set_task_id(taskId);
  qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(std::string("tester.tasks.queue"));

  mqss::qrmci::CommunicationHandler communicationHandler(
      config.common.connection);
  auto sent = communicationHandler.send(qtask, config.common.qrmciQueue);
  ASSERT_TRUE(sent) << sent.error().detail;

  auto received = communicationHandler.receive<mqss::QuantumResult>(
      std::string("tester.tasks.queue"), mqss::qrmci::test::kResultTimeout);
  ASSERT_TRUE(received) << received.error().detail;
  ASSERT_TRUE(received->has_value()) << "No result received from the queue.";

  const mqss::QuantumResult &taskResult = received->value();
  EXPECT_EQ(taskResult.task_id(), taskId);
}

} // namespace

int main(int argc, char **argv) {
  std::vector<mqss::qrmci::test::DaemonProcess> daemons;
  daemons.emplace_back("qrmcid-standalone", QRMCI_STANDALONE_DAEMON_PATH,
                       QRMCI_DAEMON_LOG_DIR);
  return mqss::qrmci::test::runWithDaemons(argc, argv, std::move(daemons));
}
