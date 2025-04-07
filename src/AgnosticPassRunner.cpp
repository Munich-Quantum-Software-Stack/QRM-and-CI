/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */
#include "mqss/ConnectionHandler.hpp"
#include "mqss/LoggerHandler.hpp"
#include "mqss/QuantumResourceManager/PassRunner.hpp"
#include "mqss/common/Logger.hpp"
#include "mqss/common/QuantumTask.hpp"
#include "mqss/common/RabbitMQServer.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <thread>
// llvm includes
#include "llvm/Bitcode/BitcodeReader.h"

#include <llvm/Support/Base64.h>
// mlir includes
#include "mlir/ExecutionEngine/OptUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Parser/Parser.h"
// cudaq includes
#include "Passes/Transforms.hpp"
#include "common/JIT.h"
#include "common/RuntimeMLIR.h"
#include "cudaq/Optimizer/CodeGen/Pipelines.h"

using json = nlohmann::json;
using namespace mlir;
using namespace mqss;
// Start threads to consume from each queue concurrently
std::vector<std::thread> threadsConnections;
void joinThreadsConnections() {
  for (auto &thread : threadsConnections)
    if (thread.joinable())
      thread.join();
}

namespace mlir {
void registerPasses() {
  // Register the passes from the TableGen-generated code
  registerMQSSOptTransformsPasses();
}
} // namespace mlir

std::tuple<mlir::ModuleOp, mlir::MLIRContext *>
extractMLIRContext(const std::string &quakeModule) {
  auto contextPtr = cudaq::initializeMLIR();
  mlir::MLIRContext &context = *contextPtr.get();

  // Get the quake representation of the kernel
  auto quakeCode = quakeModule;
  auto m_module = mlir::parseSourceString<mlir::ModuleOp>(quakeCode, &context);
  if (!m_module)
    throw std::runtime_error("Module cannot be parsed");
  return std::make_tuple(m_module.release(), contextPtr.release());
}

void handleQuantumDaemon(amqp_connection_state_t &conn, char const *QDQueue,
                         QuantumTask &parentQuantumTask) {
  auto logger = mqss::Logger::getLogger();
  int err;
  auto start = std::chrono::steady_clock::now();
  std::vector<mlir::ModuleOp> mlirCircuits;

  logger->info("Quantum Daemon");
  logger->info("Quantum Tasks:");
  // registering mqss passes
  mlir::registerPasses();
  for (std::string circuit : parentQuantumTask.circuit_files) {
    // here parse each quantum task
    // get the mlir module of the given quantum kernel
    auto [quakeModule, contextPtr] = extractMLIRContext(circuit);
    mlirCircuits.push_back(quakeModule);
    // #ifdef DEBUG
    quakeModule->dump();
    // #endif
  }
  // First getting the mlir context to create the pass manager
  QRM::PassRunner passRunner;
  // for(auto quakeModule : mlirCircuits){
  //   passRunner.applyOptimizationLevel(quakeModule,
  //                                     parentQuantumTask.optimisation_level);
  //   // #ifdef DEBUG
  //   std::cout << "Circuit after " << parentQuantumTask.optimisation_level
  //           << ":\n";
  //   quakeModule->dump();
  // }
  std::vector<std::string> passes = {"CancellationDoubleCx", "canonicalize",
                                     "cse"};
  // std::vector<std::string> passes = {"canonicalize", "cse"};
  for (auto quakeModule : mlirCircuits) {
    passRunner.invokePasses(quakeModule, passes);
    // #ifdef DEBUG
    std::cout << "Circuit after custom passes:\n";
    quakeModule->dump();
  }
  // #endif
  // Invoke the generator
  // Invoke the scheduler
  // Compile and execute each generated sub-circuit
  // Invoke the target-specific passes
  for (auto quakeModule : mlirCircuits) {
    passRunner.invokePasses(quakeModule, passes, "device");
  }
  // Submission
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
  // Create JSON string to send back to the Quantum Daemon
  json QuantumResult_json = {
      {"task_id", -1},
      //{"results", results},
      {"destination", ""},
      {"execution_status", true},
      //{"executed_qpu", targets},
      //{"executed_circuit", modules},
      {"additional_information", ""},
      {"execution_time", elapsed_milliseconds.count()},
  };
  // return the same quantum task but with results
  std::string QuantumResult_str = QuantumResult_json.dump();
  // send_message(&conn, QuantumResult_str.c_str(), QDQueue);
}

/**
 * @brief Function for the graceful termination of this daemon closing
 * its own socket before exiting
 * @param signum Number of the interrupt signal
 */
void signalHandler(int signum) {
  auto logger = mqss::Logger::getLogger();
  if (signum == SIGINT) {
    int err;
    logger->warn("Stopping the Agnostic Pass Runner");
    // Close the connections
    logger->warn("Closing connections to RabbitMQ");
    joinThreadsConnections();
    mqss::Logger::cleanup();
    exit(0);
  }
}

void applyTargetAgnosticPasses(QuantumTask quantumTask) {
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT,
                              QUEUE_AGNOSTIC_PASS_RUNNER_SCHEDULER, AMQP_USER,
                              AMQP_PASSWORD);
  std::cout << "Received task with id: " << quantumTask.task_id << std::endl;
  for (auto task : quantumTask.circuit_files)
    std::cout << task << std::endl;
  json taskJson = dumpQuantumTaskToJson(quantumTask);
  // the functionaliyt of the pass runner goes here!
  // after processing, move forward the quantum task
  forwardQueue.publishMessage(taskJson.dump(), true);
}

/**
 * @brief The main entry point of the program.
 *
 * The Quantum Resource Manager daemon.
 *
 * @return int
 */
int main(int argc, char *argv[]) {
  // Install the signal handler for SIGINT (Ctrl+C)
  std::signal(SIGINT, signalHandler);
  mqss::Logger::init(FILE_LOGGER_AGNOSTIC_PASS_RUNNER,
                     LOGGER_AGNOSTIC_PASS_RUNNER);
  // Get the logger instance
  auto logger = mqss::Logger::getLogger();
  RabbitMQServer queueListener(AMQP_SERVER, AMQP_PORT,
                               QUEUE_QRM_AGNOSTIC_PASS_RUNNER, AMQP_USER,
                               AMQP_PASSWORD);
  logger->info("Running up the Target Agnostic Pass Runner");
  queueListener.startToConsume();
  while (true) {
    logger->info("Waiting for a new job...");
    std::string message;
    queueListener.consumeMessage(message);
    QuantumTask quantumTask = dumpJsonToQuantumTask(message.c_str());
    std::string taskId = boost::uuids::to_string(quantumTask.task_id);
    threadsConnections.push_back(
        std::thread(applyTargetAgnosticPasses, std::move(quantumTask)));
    logger->info("Processing new task with id: {}", taskId);
  }
  // Ensure the logger is properly destroyed
  joinThreadsConnections();
  mqss::Logger::cleanup();
  return 1;
}
