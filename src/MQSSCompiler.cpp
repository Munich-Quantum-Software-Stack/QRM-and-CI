#include "Compiler.h"

using namespace mqss;
// Invokes cudaq-quake on `src_path` which convert the source to quake mlir
// dialect. Then mqss-cudaq-opt is called to invoke mqss-passes on the quake
// dialect Finally, cudaq-translate is called to translate the output to qasm or
// qir. Returns the raw Quake MLIR as a string. result_type can be "qir",
// "qir-full", "qir-adaptive", "qir-base", "openqasm2"
std::string lowerToOutputFormat(const std::string &srcPath, int opt_level,
                                std::string target_qpu,
                                std::string result_type = "qir-base") {
  // Write output to a temp file
  char tmpPath[] = "/tmp/mqss_quake_XXXXXX";
  int fd = mkstemp(tmpPath);
  if (fd < 0)
    throw std::runtime_error("mkstemp failed");
  close(fd);

  std::string decomposition_cmd;
  if (target_qpu == "iqm")
    decomposition_cmd = "--iqm-gate-set-mapping";
  else if (target_qpu == "fermioniq")
    decomposition_cmd = "--fermioniq-gate-set-mapping";
  else if (target_qpu == "ionq")
    decomposition_cmd = "--ionq-gate-set-mapping";
  else if (target_qpu == "oqc")
    decomposition_cmd = "--oqc-gate-set-mapping";
  else
    throw std::runtime_error("Unknown target qpu selected: " + target_qpu);

  std::string cmd =
      std::string(CUDAQ_QUAKE_PATH) + " " + srcPath + " | " +
      std::string(MQSS_CUDAQ_PATH) + " --O" + std::to_string(opt_level) +
      " | " + std::string(CUDAQ_OPT_PATH) + " " + decomposition_cmd + " | " +
      std::string(CUDAQ_TRANSLATE_PATH) + " --convert-to=" + result_type +
      " -o " + tmpPath; // capture stderr too for diagnostics

  int ret = std::system(cmd.c_str());
  if (ret != 0) {
    std::remove(tmpPath);
    throw std::runtime_error("MQSS Compiler pipeline failed with code: " +
                             std::to_string(ret));
  }

  // Read the qasm/qir output
  FILE *f = std::fopen(tmpPath, "r");
  std::string result;
  std::array<char, 4096> buf;
  while (std::fgets(buf.data(), buf.size(), f))
    result += buf.data();
  std::fclose(f);
  std::remove(tmpPath);
  return result;
}

static void applyOptimizationPasses(mqss::QuantumTask &Qtask) {

  std::unordered_map<int, std::string> updated_circuit_files;
  auto input_circuit_files = Qtask.circuit_files();

  for (unsigned i = 0; i < input_circuit_files.size(); ++i) {
    auto circuit_file = input_circuit_files[i];
    auto opt_level = Qtask.optimisation_level();
    auto qpu = Qtask.preferred_qpu();
    // Final argument to lowerToOutputFormat can be set to:
    // "qir", "qir-full", "qir-adaptive", "qir-base", "openqasm2"
    auto out_res = lowerToOutputFormat(circuit_file, opt_level, qpu);
    updated_circuit_files[i] = out_res;
  }
  for (auto [index, new_circuit_file] : updated_circuit_files) {
    Qtask.set_circuit_files(index, new_circuit_file);
  }
}

int main() {
  mqss::Logger::init(FILE_LOGGER_MQSSCompiler, LOGGER_MQSSCompiler);
  auto logger = Logger::getLogger();
  logger->info("Running up the MQSS Compiler");

  mqss::QuantumTask task;

  mqss::TransportOptions<mqss::RabbitMqSimple> opts;
  opts.host = AMQP_SERVER;
  opts.port = AMQP_PORT;
  opts.username = AMQP_USER;
  opts.password = AMQP_PASSWORD;

  mqss::Messenger<mqss::RabbitMqSimple, mqss::ProtoJson> messenger(opts);

  // Poll for incoming tasks
  while (true) {
    logger->info("Waiting for a new job...");
    auto res = messenger.receive<mqss::QuantumTask>(
        {"compiler.tasks.queue"},
        mqss::ReceiveArgs{
            .timeout = std::chrono::milliseconds(5000),
            .ack_mode = mqss::AckMode::Auto,
        });

    if (!res.has_value()) {
      // Timeout is normal — just keep polling
      if (res.error().code() == mqss::StatusCode::Timeout)
        continue;
      logger->error("Receive error: ", res.error().reason());
      continue;
    }

    mqss::QuantumTask &task = *res;
    logger->info("Received task: ", task.task_id());

    logger->info("Processing new task with id: {}", task.task_id());
    // Process the task...
    applyOptimizationPasses(task);

    auto send_st = messenger.send<mqss::QuantumTask>(
        {task.result_destination()}, // use the queue the daemon specified
        task);

    if (!send_st.ok())
      logger->error("Failed to send result: ", send_st.reason());
  }
  Logger::cleanup();
  return 0;
}