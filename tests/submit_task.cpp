/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci.hpp"

#include <functional>
#include <string>

int main(int argc, char **argv) {

  mqss::qrmci::initConfig(mqss::qrmci::loadConfig(argc, argv));

  auto config = mqss::qrmci::getConfig();

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
  qtask.set_circuit_file_type(std::string("mlir"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("EQE1"));
  qtask.set_no_modify(false);
  qtask.set_result_destination(std::string("tester.tasks.queue"));

  mqss::qrmci::CommunicationHandler<mqss::RabbitMqSimple, mqss::ProtoJson>
      communication_handler(config.rabbitmq);
  std::cout << "Sending task with task_id: " << qtask.task_id()
            << " to queue: " << config.queues.qrmci << std::endl;
  communication_handler.send_quantum_task(qtask, config.queues.qrmci);

  // Wait for the result from the compiler
  std::cout << "Waiting for result from queue: tester.tasks.queue" << std::endl;
  auto opt_result = communication_handler.get_next_quantum_result(
      std::string("tester.tasks.queue"), std::chrono::milliseconds(0));

  if (!opt_result.has_value()) {
    std::cerr << "No result received from the queue." << std::endl;
    return 1;
  }

  mqss::QuantumResult task_result = opt_result.value();
  assert(task_result.task_id() == 123);
  std::cout << "Received result for task_id: " << task_result.task_id()
            << std::endl;

  std::cout << task_result.DebugString() << std::endl;
}
