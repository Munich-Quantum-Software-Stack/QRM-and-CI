/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <connection_handling.hpp>

#include <GeneratorRunner.hpp>
#include <PassRunner.hpp>
#include <SchedulerRunner.hpp>
#include <SelectorRunner.hpp>

#include "../../tests/test_copy.cpp"
#include <qdmi.h>
#include <qdmi_internal.h>
#include <qinfo.h>

using json = nlohmann::json;
using llvm::orc::ThreadSafeModule;

#define CHECK_ERR(a, b)                                                        \
  {                                                                            \
    if (a != QDMI_SUCCESS) {                                                   \
      std::cout << std::endl << "[Error]: " << a << " at " << b;               \
    }                                                                          \
  }

/**
 * @todo Comment this
 */
struct QuantumTask {
  int task_id;
  int n_qbits;
  int n_shots;
  std::string circuit_file;
  std::string circuit_file_type;
  std::string result_destination;
  std::string preferred_qpu;
  std::string scheduled_qpu;
  int priority;
  int optimisation_level;
  bool no_modify;
  bool transpiler_flag;
  int result_type;
  std::string submit_time;
  std::string circuit_qiskit;
  std::string additional_information;
  std::string change_generator;
  std::string change_selector;
  std::string change_scheduler;
};

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

  json QuantumTask_json = json::parse(QuantumTask_str);

  task.task_id = QuantumTask_json["task_id"];
  task.n_qbits = QuantumTask_json["n_qbits"];
  task.n_shots = QuantumTask_json["n_shots"];
  task.circuit_file = QuantumTask_json["circuit_file"];
  task.circuit_file_type = QuantumTask_json["circuit_file_type"];
  task.result_destination = QuantumTask_json["result_destination"];
  task.preferred_qpu = QuantumTask_json["preferred_qpu"];
  task.scheduled_qpu = QuantumTask_json["scheduled_qpu"];
  task.priority = QuantumTask_json["priority"];
  task.optimisation_level = QuantumTask_json["optimisation_level"];
  task.no_modify = QuantumTask_json["no_modify"];
  task.transpiler_flag = QuantumTask_json["transpiler_flag"];
  task.result_type = QuantumTask_json["result_type"];
  task.submit_time = QuantumTask_json["submit_time"];
  task.circuit_qiskit = QuantumTask_json["circuit_qiskit"];
  task.additional_information = QuantumTask_json["additional_information"];
  task.change_selector = QuantumTask_json["change_selector"];
  task.change_scheduler = QuantumTask_json["change_scheduler"];

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
                         const QuantumTask &quantumTask) {
  // Start the session
  int err;
  QInfo info;
  QDMI_Session session = NULL;

  err = QInfo_create(&info);
  CHECK_ERR(err, "QInfo_create");

  err = QDMI_session_init(info, &session);
  // CHECK_ERR(err, "QDMI_session_init");

  // Invoke the generator
  std::string generator = quantumTask.change_generator == ""
                              ? "libgenerator_cutter.so"
                              : quantumTask.change_generator;

  std::vector<ThreadSafeModule> TSMs =
      invokeGenerator(quantumTask.circuit_qiskit, generator);

  if (TSMs.size() == 0) {
    std::cout
        << "   [qresourcemanager_d]..Warning: There was an error splitting "
           "the quantum circuit"
        << std::endl;
    return;
  }

  // Compile and execute each generated sub-circuit
  std::vector<std::string> modules;
  std::vector<std::string> targets;
  std::map<std::string, int> results;
  auto start = std::chrono::steady_clock::now();
  int job_count = 0;
  for (auto &TSM : TSMs) {
    QDMI_Job job;
    QDMI_Library lib;
    QDMI_Fragment frag;
    QDMI_Device device = (QDMI_Device)malloc(sizeof(struct QDMI_Device_impl_d));

    std::cout << std::endl;

    if (device == NULL) {
      // Finalize the session
      QDMI_session_finalize(session);

      std::cout << "   [qresourcemanager_d]..Warning: "
                << "The device could not be created" << std::endl;

      return;
    }

    // Invoke the scheduler
    std::string scheduler = quantumTask.change_scheduler == ""
                                ? "libscheduler_round_robin.so"
                                : quantumTask.change_scheduler;

    // TODO: extract those values
    int priority = 0;
    std::map<std::string, float> preferred_qpu = {{"Q20", 0.5}, {"Q5", 0.5}};
    float expected_execution_time = 0.25 * (job_count + 1);
    int task_id = job_count++;
    Job job_ = {job_count, task_id, expected_execution_time};

    if (invokeScheduler(scheduler, TSM, priority, preferred_qpu, job_) > 0) {
      // Finalize the session
      QDMI_session_finalize(session);

      std::cout << "   [qresourcemanager_d]..Warning: "
                << "There was an error obtaining the "
                << "target architecture" << std::endl;
      return;
    }

    // TODO Don't place the target architecture in the metadata
    // Fetch the target architecture from the metadata
    QirPassRunner &QPR = QirPassRunner::getInstance();
    QirMetadata &qirMetadata = QPR.getMetadata();
    auto targetArchitecture = qirMetadata.targetPlatform;
    targets.push_back(targetArchitecture);

    char *HOME = std::getenv("HOME");
    std::string libpath = std::string(HOME) + "/bin/lib/libbackend_q5.so";
    lib = find_library_by_name(libpath.c_str());
    if (!lib) {
      // Finalize the session
      QDMI_session_finalize(session);

      std::cout << "   [qresourcemanager_d]..Warning: "
                << "The backend could not be found" << std::endl;

      return;
    }
    device->library = *lib;

    // Invoke the selector
    std::string selector = quantumTask.change_selector == ""
                               ? "libselector_all.so"
                               : quantumTask.change_selector;
    std::vector<std::string> passes = invokeSelector(selector);

    if (passes.empty()) {
      // Finalize the session
      QDMI_session_finalize(session);

      std::cout << "   [qresourcemanager_d]..Warning: "
                << "No passes were selected" << std::endl;
      return;
    }

    // Invoke the passes
    invokePasses(TSM, passes, device);

    // Create a fragment
    frag = (QDMI_Fragment)malloc(sizeof(struct QDMI_Fragment_d));
    if (frag == NULL) {
      // Finalize the session
      QDMI_session_finalize(session);

      std::cout << "   [qresourcemanager_d]..Warning: "
                << "The fragment could not be created" << std::endl;

      return;
    }

    TSM.withModuleDo([&](Module &module) {
      std::string str;
      raw_string_ostream OS(str);
      OS << module;
      OS.flush();
      const char *qir = str.data();
      modules.push_back((char *)qir);
      frag->QIR_bitcode = strdup((char *)qir);
    });

    // Submit the adapted QIR to the target platform
    err = QDMI_control_submit(device, &frag, quantumTask.n_shots,
                              device->library.info, &job);
    CHECK_ERR(err, "QDMI_control_submit");

    //// Get the results back
    // for (const auto &result : partial_result)
    //     results[result.first] += result.second;

    free(frag->QIR_bitcode);
    free(frag);
    free(device);
  }

  // Finalize the session
  err = QDMI_session_finalize(session);
  CHECK_ERR(err, "QDMI_session_finalize");

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;

  // Create JSON string to send back to the Quantum Daemon
  json QuantumResult_json = {
      {"task_id", -1},
      {"results", results},
      {"destination", ""},
      {"execution_status", true},
      {"executed_qpu", targets},
      {"executed_circuit", modules},
      {"additional_information", ""},
      {"execution_time", elapsed_seconds.count()},
  };

  std::string QuantumResult_str = QuantumResult_json.dump();

  // Send the results back to the Quantum Daemon
  send_message(&conn,                     // conn
               QuantumResult_str.c_str(), // message
               QDQueue);                  // queue

  std::cout << "   [qresourcemanager_d]..Adapted QIR sent to the Quantum Daemon"
            << std::endl;
}

/**
 * @brief Function for the graceful termination of this daemon closing
 * its own socket before exiting
 * @param signum Number of the interrupt signal
 */
void signalHandler(int signum) {
  if (signum == SIGTERM) {
    std::cerr << "   [qresourcemanager_d]..Stoping" << std::endl;

    // Close the connections
    close_connections(&conn);

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
  setbuf(stdout, NULL);

  if (argc != 2 && argc != 3) {
    std::cerr << "   [qresourcemanager_d]..aemon_d [screen|log PATH]"
              << std::endl;
    return 1;
  }

  std::string stream;

  if (argc == 2) {
    stream = argv[1];
    if (stream != "screen") {
      std::cerr << "   [qresourcemanager_d]..aemon_d [screen|log PATH]"
                << std::endl;
      return 1;
    }
  }

  if (argc == 3) {
    stream = argv[1];
    if (stream != "log") {
      std::cerr << "   [qresourcemanager_d]..aemon_d [screen|log PATH]"
                << std::endl;
      return 1;
    }
  }

  // Fork the process to create a daemon
  pid_t pid = fork();

  if (pid < 0) {
    std::cerr << "   [qresourcemanager_d]..Failed to fork" << std::endl;
    return 1;
  }

  std::string filePath;

  if (stream == "log")
    filePath = std::string(argv[2]) + "/logs/qresourcemanager_d.log";

  if (pid > 0) {
    std::cout << "   [qresourcemanager_d]..To stop this daemon type: kill -15 "
              << pid << std::endl;
    if (stream == "log")
      std::cout << "   [qresourcemanager_d]..The log can be found in "
                << filePath << std::endl;

    return 0;
  }

  // Create a new session and become the session leader
  setsid();

  // Change the working directory to root to avoid locking the current
  // directory
  chdir("/");

  // Set up a signal handler for graceful termination
  signal(SIGTERM, signalHandler);

  // Set the output stream
  if (stream == "log") {
    int logFileDescriptor = -1;

    logFileDescriptor =
        open(filePath.c_str(), O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);

    if (logFileDescriptor == -1) {
      std::cerr << "   [qresourcemanager_d]..Warning: Could not open the "
                   "log file"
                << std::endl;
    } else {
      dup2(logFileDescriptor, STDOUT_FILENO);
      dup2(logFileDescriptor, STDERR_FILENO);
    }
  }

  // Establish a connection to the RabbitMQ server
  const char *QDQueue = "qd_queue";
  const char *QRMQueue = "qrm_queue";
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
    std::cout << "   [qresourcemanager_d]..Error starting to consume messages"
              << std::endl;
    return 1;
  }

  std::cout << "   [qresourcemanager_d]..Listening on queue " << QRMQueue
            << std::endl;

  while (true) {
    // Receive a QuantumTask
    auto *task = receive_message(&conn,     // conn
                                 QRMQueue); // queue
    // auto *task = run_test(argc, argv)->c_str();

    if (task) {
      QuantumTask quantumTask = JSONToQuantumTask(task);

      std::cout << "   [qresourcemanager_d]..Received a QuantumTask"
                << std::endl;

      // Create a new thread that executes 'handleQuantumDaemon' to run
      // the received scheduler, and the received selector targeting
      // the received QIR
      std::thread QuantumDaemonThread(handleQuantumDaemon, std::ref(conn),
                                      QDQueue, quantumTask);

      // Detach from this thread once done
      QuantumDaemonThread.detach();
    } else {
      std::cout << "   [qresourcemanager_d]..Error: Failed to receive the task"
                << std::endl;
    }
  }

  return 1;
}
