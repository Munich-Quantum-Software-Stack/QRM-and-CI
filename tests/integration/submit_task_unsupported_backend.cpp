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
// online. A circuit_file_type that
// mqss::qrmci::isCircuitTypeCompatibleWithFormats()
// (include/qrmci/ConstantsMapping.h) knows no formats for
// guarantees that deterministically, independent of which/how many backends
// are registered, so this test works unmodified against apps/standalone or
// apps/distributed.

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
  qtask.set_task_id(789);
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
              << " to be cancelled for lacking a suitable backend, but it "
                 "executed successfully."
              << "\n";
    return 1;
  }

  const std::string expectedPrefix = "CANCELLED: No suitable backend found";
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
