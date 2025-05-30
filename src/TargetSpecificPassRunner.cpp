/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */
#include "mqss/ConnectionHandler.hpp"
#include "mqss/LoggerHandler.hpp"
#include "mqss/PassRunner/PassRunner.hpp"
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

/**
 * @brief Function for the graceful termination of this daemon closing
 * its own socket before exiting
 * @param signum Number of the interrupt signal
 */
void signalHandler(int signum) {
  auto logger = mqss::Logger::getLogger();
  if (signum == SIGINT) {
    int err;
    logger->warn("Stopping the QRM daemon");
    // Close the connections
    logger->warn("Closing connections to RabbitMQ");
    // close_connections(&conn);
    //  Finalize the QDMI session
    logger->warn("Finalizing QDMI session");
    // err = QDMI_session_finalize(session);
    // CHECK_ERR(err, "QDMI_session_finalize");
    joinThreadsConnections();
    mqss::Logger::cleanup();
    exit(0);
  }
}

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

void applyTargetSpecificPasses(QuantumTask quantumTask) {
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT,
                              QUEUE_TRANSPILER_SUBMITTER, AMQP_USER,
                              AMQP_PASSWORD);
  std::cout << "Received task with id: " << quantumTask.task_id << std::endl;
  for (auto task : quantumTask.circuit_files)
    std::cout << task << std::endl;
  // the functionaliyt of the pass runner goes here!
  std::vector<mlir::ModuleOp> modules = getMLIRModules(quantumTask);
  QRM::PassRunner passRunner;
  passRunner.transpile(modules);
  // update the list of string modules
  std::vector<std::string> updatedCircuits;
  for (auto module : modules) {
    // Convert the module to a string
    std::string moduleOutput;
    llvm::raw_string_ostream stringStream(moduleOutput);
    module->print(stringStream);
    updatedCircuits.push_back(moduleOutput);
  }
  quantumTask.circuit_files = updatedCircuits;

  json taskJson = dumpQuantumTaskToJson(std::ref(quantumTask));
  forwardQueue.publishMessage(taskJson.dump(), true);
}

/**
 * @brief The main entry point of the program.
 *
 * Target Specific Pass Runner.
 *
 * @return int
 */
int main(int argc, char *argv[]) {
  // Install the signal handler for SIGINT (Ctrl+C)
  std::signal(SIGINT, signalHandler);
  mqss::Logger::init(FILE_LOGGER_TRANSPILER, LOGGER_TRANSPILER);
  // Get the logger instance
  auto logger = mqss::Logger::getLogger();
  RabbitMQServer queueListener(AMQP_SERVER, AMQP_PORT,
                               QUEUE_SCHEDULER_TRANSPILER, AMQP_USER,
                               AMQP_PASSWORD);
  logger->info("Running up the Target Specific Pass Runner");
  // tell the offloaderListener to start to consume
  queueListener.startToConsume();
  while (true) {
    logger->info("Waiting for a new job...");
    std::string message;
    queueListener.consumeMessage(message);
    boost::uuids::random_generator generator;
    QuantumTask quantumTask = dumpJsonToQuantumTask(message.c_str());
    std::string taskId = quantumTask.task_id;
    threadsConnections.push_back(
        std::thread(applyTargetSpecificPasses, std::move(quantumTask)));
    logger->info("Processing new task with id: {}", taskId);
  }
  // Ensure the logger is properly destroyed
  joinThreadsConnections();
  mqss::Logger::cleanup();
  return 1;
}
