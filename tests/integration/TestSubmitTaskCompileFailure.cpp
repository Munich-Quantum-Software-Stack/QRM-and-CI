/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Exercises the compilation-failure cancellation path end to end: backend
// selection must succeed (a valid, compatible preferred_qpu/circuit_file_type
// pair) so the task actually reaches mqss::qrmci::compileQuantumTask()
// (src/Runners.cpp), which then fails. An empty circuit file is used because
// it is explicitly guarded there ("Compilation failed for circuit file 0:
// circuit is empty.") -- unlike other malformed input, which reaches
// MQSSCompiler::compileSource() and, per that guard's own comment, is known
// to segfault on some invalid inputs instead of returning a diagnostic. That
// makes an empty circuit the only deterministic, crash-free way to force a
// real compile failure from this client-only test.

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

class SubmitTaskCompileFailureTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto loadedConfig = mqss::qrmci::loadConfig();
    ASSERT_TRUE(loadedConfig) << loadedConfig.error().detail;
    config = *loadedConfig;
  }

  mqss::qrmci::Config config;
};

TEST_F(SubmitTaskCompileFailureTest, CancelsTaskWithEmptyCircuit) {
  mqss::QuantumTask qtask;
  qtask.set_task_id(mqss::qrmci::test::uniqueTaskId());
  qtask.add_circuit_files(std::string(""));
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(
      std::string("tester.tasks.queue.compile_failure"));

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
      << "Expected the task to be cancelled for an empty circuit file, but "
         "it executed successfully.";

  const std::string expectedPrefix =
      "CANCELLED: Compilation failed for circuit file 0: circuit is empty.";
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
