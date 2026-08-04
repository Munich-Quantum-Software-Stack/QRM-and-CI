/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "Logger.hpp"
#include "Scheduler.hpp"
#include "qrmci.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

std::atomic<bool> g_terminate{false};

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    g_terminate.store(true, std::memory_order_release);
    spdlog::info("Received termination signal. Exiting gracefully.");
  }
}

int main(int argc, char **argv) {

  // Register and kill the application when receiving SIGINT or SIGTERM signals
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  mqss::qrmci::initConfig(mqss::qrmci::loadConfig(argc, argv));

  auto config = mqss::qrmci::getConfig();

  auto logger = mqss::qrmci::makeLogger(config.logging.compiler_logger,
                                        config.logging.compiler_log);

  // Use a process-specific default logger to avoid passing logger objects.
  spdlog::set_default_logger(logger);

  spdlog::info("Starting MQSS QRM&CI Daemon.");

  auto communication_handler =
      mqss::qrmci::CommunicationHandler<mqss::RabbitMqSimple, mqss::ProtoJson>(
          config.rabbitmq);

  mqss::Scheduler<mqss::QuantumTask> scheduler(
      mqss::SchedulingPolicy::PriorityBased);

  auto submitter = mqss::Submitter(config.submitter.qdmi_driver_name,
                                   config.submitter.qdmi_device_name,
                                   config.submitter.qdmi_client_token);

  // Example available backends. In a real-world scenario, this list could be
  // dynamically generated or fetched from a configuration file or service.
  std::vector<mqss::qrmci::Backend> available_backends = {
      {"iqm", "config.queues.iqm", mqss::qrmci::BackendType::Superconducting,
       10, 0.0},
      {"EQE1", "config.queues.EQE1", mqss::qrmci::BackendType::Superconducting,
       54, 0.0},
      {"CXX", "config.queues.CXX", mqss::qrmci::BackendType::Simulator, 5,
       0.0}};

  // Start a separate thread to handle submission of ready jobs
  // This thread will continuously check for ready jobs and submit them
  // to the submitter, sending results back via the communication handler.
  std::thread submitter_thread([&]() {
    spdlog::info("Submitter thread started. Waiting for ready jobs to submit.");
    while (!g_terminate) {
      // Check if there are any ready jobs to send
      if (auto nextJob = scheduler.getNextReadyJob()) {
        spdlog::info("Next job to send: {} with priority {}.",
                     nextJob->task_id(), nextJob->priority());
        try {
          auto quantumResult =
              mqss::qrmci::submit_quantum_task(*nextJob, submitter);

          communication_handler.send_quantum_result(
              quantumResult, nextJob->result_destination());
          spdlog::info("Results for task {} sent to destination {}.",
                       nextJob->task_id(), nextJob->result_destination());
        } catch (const std::exception &e) {
          spdlog::error("Failed to submit task {}: {}", nextJob->task_id(),
                        e.what());
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  });

  // Main loop to process incoming quantum tasks
  while (!g_terminate) {
    spdlog::info("Waiting for a new job in the queue: {}", config.queues.qrmci);

    auto opt_task = communication_handler.get_next_quantum_task(
        config.queues.qrmci, std::chrono::milliseconds(0), g_terminate);

    if (!opt_task.has_value()) {
      continue; // No task received, continue
    }

    mqss::QuantumTask task = opt_task.value();

    spdlog::info("Received task: {}. Selecting backend.", task.task_id());

    // Select backend for the task
    mqss::qrmci::select_backend(task, available_backends);

    spdlog::info("Compiling task: {}", task.task_id());

    try {
      mqss::qrmci::compile_quantum_task(task);
    } catch (const std::exception &e) {
      spdlog::error("Failed to compile task {}: {}", task.task_id(), e.what());
      continue;
    }

    spdlog::debug("-->Compiler output:");
    for (auto c : task.circuit_files()) {
      spdlog::debug(c);
    }

    scheduler.scheduleJob(task);

    spdlog::info("Task {} scheduled with priority {}.", task.task_id(),
                 task.priority());
    spdlog::debug("Current job count in scheduler: {}",
                  scheduler.getJobCount());
  }

  // Cleanup and shutdown
  spdlog::info("Shutting down MQSS QRM&CI Daemon.");
  submitter_thread.join();

  return 0;
}
