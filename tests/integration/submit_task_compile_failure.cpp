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

#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <chrono>
#include <iostream>
#include <string>

int main() {

  auto config = mqss::qrmci::loadConfig();
  if (!config) {
    std::cerr << config.error().detail << "\n";
    return 1;
  }

  mqss::QuantumTask qtask;
  qtask.set_task_id(1011);
  qtask.add_circuit_files(std::string(""));
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(
      std::string("tester.tasks.queue.compile_failure"));

  mqss::qrmci::CommunicationHandler communicationHandler(
      config->common.connection);
  std::cout << "Sending task with task_id: " << qtask.task_id()
            << " to queue: " << config->common.qrmciQueue << "\n";
  if (auto sent = communicationHandler.send(qtask, config->common.qrmciQueue);
      !sent) {
    std::cerr << sent.error().detail << "\n";
    return 1;
  }

  std::cout << "Waiting for result from queue: " << qtask.result_destination()
            << "\n";
  auto received = communicationHandler.receive<mqss::QuantumResult>(
      qtask.result_destination(), std::chrono::milliseconds(0));
  if (!received.has_value()) {
    std::cerr << received.error().detail << "\n";
    return 1;
  }
  const auto &optResult = *received;

  if (!optResult.has_value()) {
    std::cerr << "No result received from the queue." << "\n";
    return 1;
  }

  const mqss::QuantumResult &taskResult = optResult.value();
  if (taskResult.task_id() != qtask.task_id()) {
    std::cerr << "Expected task_id " << qtask.task_id() << ", got "
              << taskResult.task_id() << "\n";
    return 1;
  }

  if (taskResult.execution_status()) {
    std::cerr << "Expected task " << taskResult.task_id()
              << " to be cancelled for an empty circuit file, but it "
                 "executed successfully."
              << "\n";
    return 1;
  }

  const std::string expectedPrefix =
      "CANCELLED: Compilation failed for circuit file 0: circuit is empty.";
  if (!taskResult.additional_information().starts_with(expectedPrefix)) {
    std::cerr << "Expected additional_information to start with '"
              << expectedPrefix << "', got: '"
              << taskResult.additional_information() << "'\n";
    return 1;
  }

  std::cout << "Received expected cancellation for task_id: "
            << taskResult.task_id() << "\n";
  std::cout << taskResult.DebugString() << "\n";
}
