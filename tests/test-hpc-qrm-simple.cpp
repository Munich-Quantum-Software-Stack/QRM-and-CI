

#include "Compiler.h"
#include "LoggerHandler.hpp"
#include "Scheduler.h"
#include "common/Logger.hpp"

using namespace mqss;
int main() {
  mqss::Logger::init(FILE_LOGGER_DAEMON, LOGGER_DAEMON);
  auto logger = Logger::getLogger();
  logger->info("Running up the Daemon");

  mqss::QuantumTask task;
  mqss::TransportOptions<mqss::RabbitMqSimple> opts;
  opts.host = AMQP_SERVER;
  opts.port = AMQP_PORT;
  opts.username = AMQP_USER;
  opts.password = AMQP_PASSWORD;

  mqss::Messenger<mqss::RabbitMqSimple, mqss::ProtoJson> messenger(opts);
  const std::string queue_name = "scheduler.tasks.queue";

  task.set_task_id(120);
  task.set_n_qbits(6);
  task.set_n_shots(2048);
  task.set_optimisation_level(1);
  task.set_result_destination("test.results.queue");
  task.set_preferred_qpu(
      "iqm"); // Either iqm, fermioniq, ionq, oqc, quantinuum, qci

  std::string circuit_file_path =
      "/workspaces/QRM/benchmarks/CommuteCNOTRX.cpp";

  task.add_circuit_files(circuit_file_path);
  task.set_circuit_file_type("cpp");

  auto send_st = messenger.send<mqss::QuantumTask>({queue_name}, task);
  if (!send_st.ok()) {
    logger->error("Message could not be sent: ", send_st.reason());
    return 1;
  }

  // Block waiting for the result
  auto res = messenger.receive<mqss::QuantumTask>(
      {"test.results.queue"}, mqss::ReceiveArgs{
                                    .timeout = std::chrono::milliseconds(10000),
                                    .ack_mode = mqss::AckMode::Auto,
                                });

  logger->info("Task Received by Test/Daemon Queue!");
  if (res.has_value()) {
    const auto &decoded = *res;
    logger->info("-->Decoded task id: {}", decoded.task_id());
    logger->info("-->New Circuit files dump:\n");
    auto decoded_circuits = decoded.circuit_files();
    for (auto circuit : decoded_circuits) {
      logger->info(circuit);
    }
    // use decoded.task_id(), decoded.n_qbits(), etc.
  }
  Logger::cleanup();

  return 0;
}
