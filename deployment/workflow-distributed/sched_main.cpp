/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "Config.hpp"
#include "Logger.hpp"
#include "MQSSCompilerWrapper.hpp"
#include "SchedulerRunner.hpp"
#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"

#include <cstdlib>
#include <string>
#include <unistd.h>
#include <unordered_map>

int main(int argc, char **argv) {
  mqss::qrmci::initConfig(mqss::qrmci::loadConfig(argc, argv));

  auto config = mqss::qrmci::getConfig();

  auto logger = mqss::qrmci::makeLogger(config.logging.compiler_logger,
                                        config.logging.compiler_log);

  // Use a process-specific default logger to avoid passing logger objects.
  spdlog::set_default_logger(logger);

  spdlog::info("Starting MQSS QRM&CI (Scheduler) Daemon.");

  mqss::TransportOptions<mqss::RabbitMqSimple> opts;
  opts.host = std::string(config.rabbitmq.host);
  opts.port = config.rabbitmq.port;
  opts.username = std::string(config.rabbitmq.user);
  opts.password = std::string(config.rabbitmq.password);

  spdlog::info("Connecting to RabbitMQ at {}:{}, username: {}, password: {}",
               opts.host, opts.port, opts.username, opts.password);

  mqss::Messenger<mqss::RabbitMqSimple, mqss::ProtoJson> messenger(opts);

  while (true) {
    spdlog::info("Waiting for a new job");

    auto res = messenger.receive<mqss::QuantumTask>(
        {std::string(config.queues.scheduler)},
        mqss::ReceiveArgs{
            .timeout = std::chrono::milliseconds(5000),
            .ack_mode = mqss::AckMode::Auto,
        });

    if (!res.has_value()) {
      // Timeout is normal - just keep polling
      if (res.error().code() == mqss::StatusCode::Timeout) {
        continue;
      }

      spdlog::error("Receive error: {}", res.error().reason());
      continue;
    }

    auto task = *res;

    spdlog::info("Scheduling task: {}", task.task_id());

    try {
      mqss::qrmci::SchedulerRunner::schedule_quantum_task(task);
    } catch (const std::exception &e) {
      spdlog::error("Failed to schedule task {}: {}", task.task_id(), e.what());
      continue;
    }

    spdlog::info("-->Scheduler output:");
    spdlog::info("Scheduled QPU: {}", task.scheduled_qpu());

    auto send_st = messenger.send<mqss::QuantumTask>(
        {std::string(config.queues.compiler)}, task);

    spdlog::info("Task sent by scheduler!");
    if (!send_st.ok()) {
      spdlog::error("Failed to send result for task {}: {}", task.task_id(),
                    send_st.reason());
      continue;
    }

    spdlog::info("Result sent for task: {}", task.task_id());
  }

  return 0;
}
