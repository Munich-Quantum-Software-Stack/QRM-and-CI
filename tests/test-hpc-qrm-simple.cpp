
#include "ConnectionHandler.hpp"
#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/protocol/ProtoProtocol.hpp"
#include "mqss/transport/RabbitMqSimpleTransport.hpp"
#include "mqss/transport/Transport.hpp"
#include "common/Logger.hpp"
#include "LoggerHandler.hpp"
#include <complex>
#include <csignal>
#include <gtest/gtest.h>


using namespace mqss;
int main() {
  std::signal(SIGINT, [](int) {
    Logger::cleanup();
    exit(0);
  });

  std::signal(SIGTERM, [](int) {
    Logger::cleanup();
    exit(0);
  });

  mqss::Logger::init(TEST_CASE_LOG_FILES_PATH, LOGGER_DAEMON);
  auto logger = Logger::getLogger();
  logger->info("Running up the Daemon");

  mqss::QuantumTask task{};
  mqss::TransportOptions<mqss::RabbitMqSimple> opts;
  opts.host = AMQP_SERVER;
  opts.port = AMQP_PORT;
  opts.username = AMQP_USER;
  opts.password = AMQP_PASSWORD;

  mqss::Messenger<mqss::RabbitMqSimple, mqss::ProtoJson> messenger(opts);

  task.set_task_id(111);
  task.set_n_qbits(2);
  task.set_n_shots(0); // What should this value be? Currently,setting to 0 produces results
  task.set_optimisation_level(1);
  task.set_result_destination(RESULTS_QUEUE);
  task.set_preferred_qpu(""); // Either iqm, fermioniq, ionq, oqc, quantinuum,
                              // qci (Set to "" for example QDMI device)

  std::string circuit_file_path = "/workspaces/QRM/benchmarks/bell_state.cpp";

  task.add_circuit_files(circuit_file_path);
  task.set_circuit_file_type("cpp");

  auto send_st = messenger.send<mqss::QuantumTask>({SCHEDULER_QUEUE}, task);
  if (!send_st.ok()) {
    logger->error("Message could not be sent: ", send_st.reason());
    return 1;
  }

  logger->info("Task Sent...");
  // Block waiting for the result
  auto res = messenger.receive<mqss::QuantumResult>(
      {task.result_destination()}, mqss::ReceiveArgs{
                                    .timeout = std::chrono::milliseconds(60000),
                                    .ack_mode = mqss::AckMode::Auto,
                                });
  
  if (res.has_value()) {
    logger->info("Results Received by Test/Daemon Queue!");
    const auto &decoded = *res;
    logger->info("-->Decoded task id: {}", decoded.task_id());
    logger->info("-->Executed Circuit files dump:\n");
    auto decoded_circuits = decoded.executed_circuits();
    for (auto circuit : decoded_circuits) {
      logger->info(circuit);
    }
    logger->info(decoded.additional_information());
    // use decoded.task_id(), decoded.n_qbits(), etc.
  }
  Logger::cleanup();

  return 0;
}
