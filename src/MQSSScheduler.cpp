#include "LoggerHandler.hpp"
#include "Scheduler.h"
#include <csignal>

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

  mqss::Logger::init(SCHEDULER_LOG_FILES_PATH, LOGGER_SCHEDULER);
  auto logger = Logger::getLogger();
  logger->info("Running up the MQSS Scheduler");

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
        {SCHEDULER_QUEUE},
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
    logger->info("Received task: {}", task.task_id());

    logger->info("Forwarding task to Compiler: {}", task.task_id());

    auto send_st = messenger.send<mqss::QuantumTask>(
        {COMPILER_QUEUE}, // use the queue the daemon specified
        task);

    if (!send_st.ok())
      logger->error("Failed to send task: ", send_st.reason());
  }
  Logger::cleanup();
  return 0;
}