/* This code and any associated documentation is provided "as is"

// Copyright 2024 Munich Quantum Software Stack Project

// Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
// "License"); you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

// TODO

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -------------------------------------------------------------------------
//   author Martin Letras
//   date   April 2025
//   version 1.0
//   brief
//     Implementation of the target agnostic pass runner module.

// *******************************************************************************
// * This source code and the accompanying materials are made available under    *
// * the terms of the Apache License 2.0 which accompanies this distribution.    *
// ******************************************************************************/

// #include "mqss/ConnectionHandler.hpp"
// #include "mqss/LoggerHandler.hpp"
// #include "mqss/PassRunner/PassRunner.hpp"
// #include "mqss/common/Logger.hpp"
// #include "mqss/common/QuantumTask.hpp"
// #include "mqss/common/RabbitMQServer.hpp"

// #include <boost/uuid/uuid.hpp>
// #include <boost/uuid/uuid_generators.hpp>
// #include <boost/uuid/uuid_io.hpp>
// #include <csignal>
// #include <cstdlib>
// #include <iostream>
// #include <map>
// #include <mutex>
// #include <nlohmann/json.hpp>
// #include <shared_mutex>
// #include <thread>
// // llvm includes
// #include "llvm/Bitcode/BitcodeReader.h"

// #include <llvm/Support/Base64.h>
// // mlir includes
// #include "mlir/ExecutionEngine/OptUtils.h"
// #include "mlir/IR/BuiltinOps.h"
// #include "mlir/Parser/Parser.h"
// // cudaq includes
// #include "Passes/Transforms.hpp"
// #include "common/JIT.h"
// #include "common/RuntimeMLIR.h"
// // #include "cudaq/Optimizer/CodeGen/Pipelines.h"
// #include "amqp.h"
// using json = nlohmann::json;
// using namespace mlir;
// using namespace mqss;
// // Start threads to consume from each queue concurrently
// std::vector<std::thread> threadsConnections;
// void joinThreadsConnections() {
//   for (auto &thread : threadsConnections)
//     if (thread.joinable())
//       thread.join();
// }

// namespace mlir {
// void registerPasses() {
//   // Register the passes from the TableGen-generated code
//   registerMQSSOptTransformsPasses();
// }
// } // namespace mlir

// /*void handleQuantumDaemon(amqp_connection_state_t &conn, char const *QDQueue,
//                          QuantumTask &parentQuantumTask) {
//   auto logger = mqss::Logger::getLogger();
//   int err;
//   auto start = std::chrono::steady_clock::now();
//   std::vector<mlir::ModuleOp> mlirCircuits;

//   logger->info("Quantum Daemon");
//   logger->info("Quantum Tasks:");


//   for (std::string circuit : parentQuantumTask.circuit_files) {
//     // here parse each quantum task
//     // get the mlir module of the given quantum kernel
//     //auto [quakeModule, contextPtr] = extractMLIRContext(circuit);
//     mlirCircuits.push_back(quakeModule);
//     // #ifdef DEBUG
//     quakeModule->dump();
//     // #endif
//   }
//   // First getting the mlir context to create the pass manager
//   QRM::PassRunner passRunner;
//   // for(auto quakeModule : mlirCircuits){
//   //   passRunner.applyOptimizationLevel(quakeModule,
//   //                                     parentQuantumTask.optimisation_level);
//   //   // #ifdef DEBUG
//   //   std::cout << "Circuit after " << parentQuantumTask.optimisation_level
//   //           << ":\n";
//   //   quakeModule->dump();
//   // }
//   std::vector<std::string> passes = {"CancellationDoubleCx", "canonicalize",
//                                      "cse"};
//   // std::vector<std::string> passes = {"canonicalize", "cse"};
//   for (auto quakeModule : mlirCircuits) {
//     passRunner.invokePasses(quakeModule, passes);
//     // #ifdef DEBUG
//     std::cout << "Circuit after custom passes:\n";
//     quakeModule->dump();
//   }
//   // #endif
//   for (auto quakeModule : mlirCircuits) {
//     passRunner.invokePasses(quakeModule, passes, "device");
//   }
//   // Submission
//   auto end = std::chrono::steady_clock::now();
//   std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
//   // Create JSON string to send back to the Quantum Daemon
//   json QuantumResult_json = {
//       {"task_id", -1},
//       //{"results", results},
//       {"destination", ""},
//       {"execution_status", true},
//       //{"executed_qpu", targets},
//       //{"executed_circuit", modules},
//       {"additional_information", ""},
//       {"execution_time", elapsed_milliseconds.count()},
//   };
//   // return the same quantum task but with results
//   std::string QuantumResult_str = QuantumResult_json.dump();
//   // send_message(&conn, QuantumResult_str.c_str(), QDQueue);
// }*/

// /**
//  * @brief Function for the graceful termination of this daemon closing
//  * its own socket before exiting
//  * @param signum Number of the interrupt signal
//  */
// void signalHandler(int signum) {
//   auto logger = mqss::Logger::getLogger();
//   if (signum == SIGINT) {
//     int err;
//     logger->warn("Stopping the Agnostic Pass Runner");
//     // Close the connections
//     logger->warn("Closing connections to RabbitMQ");
//     joinThreadsConnections();
//     mqss::Logger::cleanup();
//     exit(0);
//   }
// }

// void applyTargetAgnosticPasses(QuantumTask quantumTask) {
//   RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT,
//                               QUEUE_AGNOSTIC_PASS_RUNNER_SCHEDULER, AMQP_USER,
//                               AMQP_PASSWORD);
// #ifdef DEBUG
//   std::cout << "Received task with id: " << quantumTask.task_id << std::endl;
//   for (auto task : quantumTask.circuit_files)
//     std::cout << task << std::endl;
// #endif
//   // the functionaliyt of the pass runner goes here!
//   std::vector<mlir::ModuleOp> modules = getMLIRModules(quantumTask);
//   QRM::PassRunner passRunner;
//   passRunner.applyOptimizationLevel(modules, quantumTask.optimisation_level);
//   std::vector<std::string> passes = {"canonicalize", "cse"};
//   passRunner.invokePasses(modules, passes);
//   // update the list of string modules
//   std::vector<std::string> updatedCircuits;
//   for (auto module : modules) {
//     // Convert the module to a string
//     std::string moduleOutput;
//     llvm::raw_string_ostream stringStream(moduleOutput);
//     module->print(stringStream);
//     updatedCircuits.push_back(moduleOutput);
//   }
//   quantumTask.circuit_files = updatedCircuits;

//   // after processing, move forward the quantum task
//   json taskJson = dumpQuantumTaskToJson(quantumTask);
//   forwardQueue.publishMessage(taskJson.dump(), true);
// }

// /**
//  * @brief The main entry point of the program.
//  *
//  * The Quantum Resource Manager daemon.
//  *
//  * @return int
//  */
// int main(int argc, char *argv[]) {
//   // Install the signal handler for SIGINT (Ctrl+C)
//   std::signal(SIGINT, signalHandler);
//   mqss::Logger::init(FILE_LOGGER_AGNOSTIC_PASS_RUNNER,
//                      LOGGER_AGNOSTIC_PASS_RUNNER);
//   // Get the logger instance
//   auto logger = mqss::Logger::getLogger();
//   RabbitMQServer queueListener(AMQP_SERVER, AMQP_PORT,
//                                QUEUE_QRM_AGNOSTIC_PASS_RUNNER, AMQP_USER,
//                                AMQP_PASSWORD);
//   // registering mqss passes
//   mlir::registerPasses();
//   logger->info("Running up the Target Agnostic Pass Runner");
//   queueListener.startToConsume();
//   while (true) {
//     logger->info("Waiting for a new job...");
//     std::string message;
//     queueListener.consumeMessage(message);
//     QuantumTask quantumTask = dumpJsonToQuantumTask(message.c_str());
//     std::string taskId = quantumTask.task_id;
//     threadsConnections.push_back(
//         std::thread(applyTargetAgnosticPasses, std::move(quantumTask)));
//     logger->info("Processing new task with id: {}", taskId);
//   }
//   // Ensure the logger is properly destroyed
//   joinThreadsConnections();
//   mqss::Logger::cleanup();
//   return 1;
// }


#include <fstream>
#include <iostream>
#include "mqss/protocol/ProtoProtocol.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <utility>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <array>


using namespace mqss;

// Invokes cudaq-quake on `src_path` which convert the source to quake mlir dialect.
// Then mqss-cudaq-opt is called to invoke mqss-passes on the quake dialect
// Finally, cudaq-translate is called to translate the output to qasm or qir.
// Returns the raw Quake MLIR as a string.
// result_type can be "qir", "qir-full", "qir-adaptive", "qir-base", "openqasm2"
std::string lowerToOutputFormat(const std::string &srcPath, int opt_level,
                           std::string target_qpu,
                           std::string result_type = "qir-base") {
  // Write output to a temp file
  char tmpPath[] = "/tmp/mqss_quake_XXXXXX";
  int fd = mkstemp(tmpPath);
  if (fd < 0)
    throw std::runtime_error("mkstemp failed");
  close(fd);

  std::string cmd =
      std::string(CUDAQ_QUAKE_PATH) + " " + srcPath + " | " +
      std::string(MQSS_CUDAQ_PATH) + " --O" + std::to_string(opt_level) + " | " +
      std::string(CUDAQ_TRANSLATE_PATH) + " --convert-to=" + result_type
       + " -o " +
      tmpPath; // capture stderr too for diagnostics

  int ret = std::system(cmd.c_str());
  if (ret != 0) {
    std::remove(tmpPath);
    throw std::runtime_error("MQSS Compiler pipeline failed with code: " +
                             std::to_string(ret));
  }

  // Read the .qke output
  FILE *f = std::fopen(tmpPath, "r");
  std::string result;
  std::array<char, 4096> buf;
  while (std::fgets(buf.data(), buf.size(), f))
    result += buf.data();
  std::fclose(f);
  std::remove(tmpPath);
  return result;
}

void applyOptimizationPasses(const QuantumTask &Qtask) {

  std::cout << "-->Transforming all circuits:\n";
  for (auto circuit_path : Qtask.circuit_files()) {
    auto opt_level = Qtask.optimisation_level();
    auto qpu = Qtask.preferred_qpu();
    // Final argument to lowerToOutputFormat can be set to:
    // "qir", "qir-full", "qir-adaptive", "qir-base", "openqasm2"
    auto out_res = lowerToOutputFormat(circuit_path, opt_level, qpu);
    std::cout << out_res << "\n";
  }
}

int main() {
  std::cout << "Hello from the MQSSCompiler!\n";
  mqss::QuantumTask task;
  task.set_task_id(120);
  task.set_n_qbits(6);
  task.set_n_shots(2048);
  task.set_optimisation_level(1);
  task.set_result_destination("results.extended.queue");
  task.set_preferred_qpu("iqm");            // Either iqm, fermioniq, ionq, oqc, quantinuum, qci
  
  std::string circuit_file_path = "/workspaces/QRM/benchmarks/CommuteCNOTRX.cpp";
  // std::ifstream file(circuit_file_path);
  // if (!file) {
  //   std::cerr << "Failed to open file." << std::endl;
  //   return 1;
  // }
  // std::stringstream bufferFile;
  // bufferFile << file.rdbuf();                  // Read file content into buffer
  // std::string circuit_file = bufferFile.str(); // Convert buffer to string

  task.add_circuit_files(circuit_file_path);
  task.set_circuit_file_type("cpp");

  applyOptimizationPasses(task);
}