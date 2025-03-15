/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */
#include "mqss/QuantumResourceManager.hpp"

#include "mqss/Utils/Logger.hpp"

using json = nlohmann::json;

/**
 * @var conn
 * @brief TODO
 */
amqp_connection_state_t conn;

/**
 * @todo Comment this function
 */
QuantumTask JSONToQuantumTask(const char *QuantumTask_str) {
  QuantumTask task;
  auto logger = Logger::getLogger();
  json QuantumTask_json = json::parse(QuantumTask_str);

  if (!QuantumTask_json.contains("task_id")) {
    logger->warn("task_id not defined in json file");
    return QuantumTask();
  }
  task.task_id = QuantumTask_json["task_id"];
  if (!QuantumTask_json.contains("parent_id"))
    task.parent_id = -1;
  else
    task.parent_id = QuantumTask_json["parent_id"];
  task.n_qbits = QuantumTask_json["n_qbits"];
  task.n_shots = QuantumTask_json["n_shots"];
  task.circuit_file = QuantumTask_json["circuit_file"];
  task.circuit_file_type = QuantumTask_json["circuit_file_type"];
  task.result_destination = QuantumTask_json["result_destination"];
  task.preferred_qpu = QuantumTask_json["preferred_qpu"];
  task.priority = QuantumTask_json["priority"];
  task.optimisation_level = QuantumTask_json["optimisation_level"];
  task.no_modify = QuantumTask_json["no_modify"];
  task.transpiler_flag = QuantumTask_json["transpiler_flag"];
  task.result_type = QuantumTask_json["result_type"];
  task.submit_time = QuantumTask_json["submit_time"];
  if (!QuantumTask_json.contains("quake")) {
    logger->warn("MLIR circuit missing!");
    return QuantumTask();
  }
  task.quake = QuantumTask_json["quake"];
  task.additional_information = QuantumTask_json["additional_information"];
  //    task.thread_safe_module = ThreadSafeModule();
  return task;
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
  auto logger = Logger::getLogger();
  logger->info("Quantum Daemon");
  int err;
  auto start = std::chrono::steady_clock::now();
  logger->info("Quantum Task:");
  std::cout << parentQuantumTask.quake << std::endl;
  // Invoke the target-agnostic selector
  // Invoke the generator
  // Invoke the scheduler
  // Compile and execute each generated sub-circuit
  // Invoke the target-specific selector
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
  std::string QuantumResult_str = QuantumResult_json.dump();
  send_message(&conn, QuantumResult_str.c_str(), QDQueue);
}

/**
 * @brief Function for the graceful termination of this daemon closing
 * its own socket before exiting
 * @param signum Number of the interrupt signal
 */
void signalHandler(int signum) {
  auto logger = Logger::getLogger();
  if (signum == SIGTERM) {
    int err;
    logger->error("Stopping the QRM daemon");
    // Close the connections
    logger->error("Closing connections to RabbitMQ");
    close_connections(&conn);
    // Finalize the QDMI session
    logger->error("Finalizing QDMI session");
    // err = QDMI_session_finalize(session);
    // CHECK_ERR(err, "QDMI_session_finalize");
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
  Logger::init();
  // Get the logger instance
  auto logger = Logger::getLogger();
  logger->info("Running up the Quantum Resource Manager (QRM)");
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
  return 1;
}
