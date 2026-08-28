/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "Submitter.h"
#include "mqss/protocol/ProtoProtocol.hpp"
#include "qrmci/BackendWrapper.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"
#include "qrmci/Logger.h"
#include "qrmci/Runners.h"
#include "scheduler/scheduler.hpp"

#include <atomic>
#include <cassert>
#include <cmath>
#include <csignal>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace {
/*
 * The terminationFlag() function returns a reference to a static atomic
 * boolean. This static variable is shared across the application and can be
 * checked in the main loop to determine if the application should terminate
 * gracefully. When a termination signal (SIGINT or SIGTERM) is received, the
 * signalHandler function sets this flag to true.
 */
std::atomic<bool> &terminationFlag() {
  static std::atomic<bool> flag{false};
  return flag;
}

/*
 * Signal handler for SIGINT and SIGTERM signals.
 * When a termination signal is received, this function sets the termination
 * flag to true, which can be checked in the main loop to terminate the
 * application gracefully.
 */
void signalHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    terminationFlag().store(true, std::memory_order_release);
  }
}
} // namespace

int main(int argc, char **argv) {

  // Register and kill the application when receiving SIGINT or SIGTERM signals
  // This allows the application to terminate gracefully when the user presses
  // Ctrl+C or when the system sends a termination signal.
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Initialize the configuration and logger for the QRM&CI daemon
  mqss::qrmci::initConfig(mqss::qrmci::loadConfig(argc, argv));
  const mqss::qrmci::Config &config = mqss::qrmci::getConfig();
  std::shared_ptr<spdlog::logger> logger = mqss::qrmci::makeLogger(
      config.logging.daemonLogger, config.logging.daemonLog);
  spdlog::set_default_logger(logger);

  // Initialize the communication handler for RabbitMQ with ProtoJson
  // serialization This handler will be used to receive quantum tasks and send
  // results back.
  auto communicationHandler =
      mqss::qrmci::CommunicationHandler<mqss::RabbitMqSimple, mqss::ProtoJson>(
          config.rabbitmq);

  // Initialize the scheduler with a priority-based scheduling policy. The
  // scheduler will manage the execution order of quantum tasks based on their
  // priority.
  mqss::Scheduler<mqss::QuantumTask> scheduler(
      mqss::SchedulingPolicy::PriorityBased);

  // Initialize the submitter to connect to the QDMI backend using the provided
  // driver name, device name, device ID, and authentication token. The
  // submitter will be used to submit quantum tasks to the backend for
  // execution.
  auto submitter = mqss::submitter::Submitter(
      config.submitter.qdmiDriverName, config.submitter.qdmiDeviceName,
      config.submitter.qdmiDeviceName, config.submitter.qdmiClientToken);

  spdlog::info("Connected to QDMI device: {} using driver: {}",
               config.submitter.qdmiDeviceName,
               config.submitter.qdmiDriverName);

  // Discover available backends and store them in a map for quick access. Each
  // backend is wrapped in a BackendWrapper object that provides convenient
  // access to its properties and methods.
  std::unordered_map<std::string, mqss::qrmci::BackendWrapper>
      availableBackends{};
  auto cxxBackend = mqss::qrmci::BackendWrapper(submitter);
  availableBackends[cxxBackend.getName()] = cxxBackend;

  spdlog::info("Available backends:");
  for (const auto &[name, backend] : availableBackends) {
    spdlog::info("Backend: {}, Qubits: {}, Status: {}", name,
                 backend.getNumQubits(), static_cast<int>(backend.getStatus()));
  }

  spdlog::info("Starting MQSS QRM&CI Daemon.");

  // Main loop to process incoming quantum tasks
  while (!terminationFlag()) {
    spdlog::info("Waiting for a new job in the queue: '{}'",
                 config.queues.qrmci);
    std::optional<mqss::QuantumTask> nextTask =
        communicationHandler.getNextQuantumTask(config.queues.qrmci,
                                                std::chrono::milliseconds(0),
                                                terminationFlag());
    if (nextTask) {
      mqss::QuantumTask task = nextTask.value();
      spdlog::info("Received task: {}. Selecting backend.", task.task_id());

      auto backendSelectedAndCompiled =
          mqss::qrmci::selectBackend(task, availableBackends)
              .and_then([&task, &availableBackends]() {
                spdlog::info("Selected backend: {}. Compiling task: {}",
                             task.scheduled_qpu(), task.task_id());
                return mqss::qrmci::compileQuantumTask(
                    task, availableBackends[task.scheduled_qpu()]);
              })
              .and_then([&task, &scheduler]() {
                spdlog::info("Task {} compiled. Scheduling for execution.",
                             task.task_id());
                scheduler.scheduleTask(task);

                spdlog::debug("Task {} scheduled with priority {}. total "
                              "scheduled jobs: {}",
                              task.task_id(), task.priority(),
                              scheduler.getTaskCount());
                return std::expected<void, std::string>{};
              });

      if (!backendSelectedAndCompiled) {
        communicationHandler.sendQuantumResult(
            mqss::qrmci::cancelQuantumTask(task,
                                           backendSelectedAndCompiled.error()),
            task.result_destination());
        spdlog::warn(
            "Task {} could not be scheduled due to backend selection or "
            "compilation failure: {}",
            task.task_id(), backendSelectedAndCompiled.error());
      }
    }

    // Check if there are any ready jobs to execute and submit them to the
    // backend
    while (auto nextJob = scheduler.getNextReadyTask()) {
      spdlog::info("Next job to execute: {} with priority {}.",
                   nextJob->task_id(), nextJob->priority());

      auto executionResult =
          mqss::qrmci::executeQuantumTask(*nextJob, submitter);

      if (executionResult) {
        communicationHandler.sendQuantumResult(executionResult.value(),
                                               nextJob->result_destination());
        spdlog::info(
            "Job Executed successfully. Results for task {} sent to '{}'.",
            nextJob->task_id(), nextJob->result_destination());
      } else { // If execution fails, send a cancellation result back
        communicationHandler.sendQuantumResult(
            mqss::qrmci::cancelQuantumTask(*nextJob, executionResult.error()),
            nextJob->result_destination());
        spdlog::warn("Execution failed for task {}: {}", nextJob->task_id(),
                     executionResult.error());
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  spdlog::info("Shutting down MQSS QRM&CI Daemon.");

  return 0;
}
