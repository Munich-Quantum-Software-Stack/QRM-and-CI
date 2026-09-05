/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "IntegrationTestHelpers.h"
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
  qtask.set_task_id(123);
  qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
  // qtask.set_preferred_qpu(std::string("EQE1"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(std::string("tester.tasks.queue"));

  mqss::qrmci::CommunicationHandler communicationHandler(
      config->common.connection);
  std::cout << "Sending task with task_id: " << qtask.task_id()
            << " to queue: " << config->common.qrmciQueue << "\n";
  if (auto sent = communicationHandler.send(qtask, config->common.qrmciQueue);
      !sent) {
    std::cerr << sent.error().detail << "\n";
    return 1;
  }

  // Wait for the result from the compiler
  std::cout << "Waiting for result from queue: tester.tasks.queue" << "\n";
  auto received = communicationHandler.receive<mqss::QuantumResult>(
      std::string("tester.tasks.queue"), std::chrono::milliseconds(0));
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
  if (taskResult.task_id() != 123) {
    std::cerr << "Expected task_id 123, got " << taskResult.task_id() << "\n";
    return 1;
  }
  std::cout << "Received result for task_id: " << taskResult.task_id() << "\n";

  std::cout << taskResult.DebugString() << "\n";
}
