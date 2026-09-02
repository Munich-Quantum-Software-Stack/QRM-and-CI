/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

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

  std::string _circuit_str = R"(
  func.func @__nvqpp__mlirgen__testILm2EE() attributes {"cudaq-entrypoint", "cudaq-kernel"} {
  %0 = quake.alloca !quake.veq<2>
  %1 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x %1 : (!quake.ref) -> ()
  %2 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %3 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%2] %3 : (!quake.ref, !quake.ref) -> ()
  %4 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %5 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%4] %5 : (!quake.ref, !quake.ref) -> ()
  %6 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %7 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%6] %7 : (!quake.ref, !quake.ref) -> ()
  %8 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %9 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%8] %9 : (!quake.ref, !quake.ref) -> ()
  %10 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %11 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%10] %11 : (!quake.ref, !quake.ref) -> ()
  %12 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x %12 : (!quake.ref) -> ()
  %13 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  %14 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  quake.x [%13] %14 : (!quake.ref, !quake.ref) -> ()
  %measOut = quake.mz %0 : (!quake.veq<2>) -> !cc.stdvec<!quake.measure>
  return
  }
  )";

  mqss::QuantumTask qtask;
  qtask.set_task_id(123);
  qtask.add_circuit_files(_circuit_str);
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
