/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */
#include "mqss/QuantumResourceManager.hpp"

#include "mqss/QuantumResourceManager/PassRunner.hpp"
#include "mqss/Utils/Logger.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
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

namespace mlir {
void registerPasses() {
  // Register the passes from the TableGen-generated code
  registerMQSSOptTransformsPasses();
}
} // namespace mlir

/**
 * @var conn
 * @brief TODO
 */
amqp_connection_state_t conn;
/**
 * @todo Comment this function
 */
QuantumTask JSONToQuantumTask(const char *QuantumTask_str) {
  QuantumTask qt;
  auto logger = mqss::Logger::getLogger();
  json jsonQT = json::parse(QuantumTask_str);

  if (!jsonQT.contains("task_id")) {
    logger->warn("task_id not defined in json file");
    return QuantumTask();
  }
  jsonQT.at("task_id").get_to(qt.task_id);
  jsonQT.at("n_qbits").get_to(qt.n_qbits);
  jsonQT.at("n_shots").get_to(qt.n_shots);
  if (!jsonQT.contains("circuit_files")) {
    logger->warn("circuit_files not defined in json file");
    return QuantumTask();
  }
  jsonQT.at("circuit_files").get_to(qt.circuit_files);
  jsonQT.at("circuit_file_type").get_to(qt.circuit_file_type);
  jsonQT.at("result_destination").get_to(qt.result_destination);
  jsonQT.at("preferred_qpu").get_to(qt.preferred_qpu);
  jsonQT.at("scheduled_qpu").get_to(qt.scheduled_qpu);
  jsonQT.at("priority").get_to(qt.priority);
  jsonQT.at("optimisation_level").get_to(qt.optimisation_level);
  jsonQT.at("no_modify").get_to(qt.no_modify);
  jsonQT.at("transpiler_flag").get_to(qt.transpiler_flag);
  jsonQT.at("result_type").get_to(qt.result_type);
  jsonQT.at("submit_time").get_to(qt.submit_time);
  jsonQT.at("circuits_qiskit").get_to(qt.circuits_qiskit);
  jsonQT.at("additional_information").get_to(qt.additional_information);
  jsonQT.at("restricted_resource_names").get_to(qt.restricted_resource_names);
  jsonQT.at("user_identity").get_to(qt.user_identity);
  jsonQT.at("token").get_to(qt.token);
  jsonQT.at("via_hpc").get_to(qt.via_hpc);
  return qt;
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

/**
 * @brief TODO
 * @param conn TODO
 * @param QDQueue TODO
 * @param receivedQirModule TODO
 * @param receivedScheduler TODO
 * @param receivedSelector TODO
 */
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
  send_message(&conn, QuantumResult_str.c_str(), QDQueue);
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
    logger->warn("Stopping the QRM daemon");
    // Close the connections
    logger->warn("Closing connections to RabbitMQ");
    close_connections(&conn);
    // Finalize the QDMI session
    logger->warn("Finalizing QDMI session");
    // err = QDMI_session_finalize(session);
    // CHECK_ERR(err, "QDMI_session_finalize");
    mqss::Logger::cleanup();
    exit(0);
  }
}

/**
 * @brief The main entry point of the program.
 *
 * The Quantum Resource Manager daemon.
 *
 * @return int
 */
int main(int argc, char *argv[]) {
  mqss::Logger::init("QRM-log.txt", "QRM-logger");
  // Get the logger instance
  auto logger = mqss::Logger::getLogger();
  logger->info("Running up the Quantum Resource Manager (QRM)");
  // Install the signal handler for SIGINT (Ctrl+C)
  std::signal(SIGINT, signalHandler);

  // Log some messages
  //  logger->info("This is an info message.");
  //  logger->warn("This is a warning message.");
  //  logger->error("This is an error message.");
  //  logger->debug("This a debug message");
  //  logger->trace("This is a trace message");

  // Establish a connection to the RabbitMQ server
  const char *QDQueue = "queue_daemon";
  const char *QRMQueue = "queue_manager";
  amqp_socket_t *socket = NULL;
  rabbitmq_new_connection(&conn, &socket);

  // Declare the Quantum Daemon queue
  amqp_queue_declare(conn, 1, amqp_cstring_bytes(QDQueue), 0, 1, 0, 0,
                     amqp_empty_table);
  // Declare the Quantum Resource Manager queue
  amqp_queue_declare(conn, 1, amqp_cstring_bytes(QRMQueue), 0, 1, 0, 0,
                     amqp_empty_table);
  amqp_rpc_reply_t consume_reply = amqp_get_rpc_reply(conn);

  if (consume_reply.reply_type != AMQP_RESPONSE_NORMAL) {
    logger->error("Error starting to consume messages");
    return 1;
  }

  // Start the QDMI session
  int err;

  while (true) {
    logger->info("Waiting for a new job...");
    // Receive a QuantumTask
    auto *task = receive_message(&conn,     // conn
                                 QRMQueue); // queue
    if (task) {
      QuantumTask parentQuantumTask = JSONToQuantumTask(task);
      logger->info("Received a QuantumTask");
      handleQuantumDaemon(conn, QDQueue, parentQuantumTask);
    } else {
      logger->error("Error: Failed to receive the task");
    }
  }
  // Ensure the logger is properly destroyed
  mqss::Logger::cleanup();
  return 1;
}
