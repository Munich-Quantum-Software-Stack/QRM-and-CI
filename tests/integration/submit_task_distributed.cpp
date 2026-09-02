/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Exercises the apps/distributed pipeline (selector -> worker) rather than
// apps/standalone. The client-visible contract is identical to
// submit_task.cpp -- tasks go in on config.common.qrmciQueue and results
// come back on the caller-supplied result_destination -- because the
// distributed selector shares that same front-door queue with the
// standalone daemon; the only difference is which deployment must be
// running for this test to pass:
// qrmcid-distributed-selector and qrmcid-distributed-worker, not the
// standalone daemon.

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
  qtask.set_task_id(456);
  qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(std::string("tester.tasks.queue.distributed"));

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

  if (!taskResult.execution_status()) {
    std::cerr << "Task " << taskResult.task_id()
              << " did not execute successfully: "
              << taskResult.additional_information() << "\n";
    return 1;
  }

  std::cout << "Received result for task_id: " << taskResult.task_id() << "\n";
  std::cout << taskResult.DebugString() << "\n";
}
