/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "Scheduler.hpp"
#include "Submitter.h"
#include "mqss/protocol/ProtoProtocol.hpp"
#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"
#include "qrmci/Error.h"
#include "qrmci/Logger.h"
#include "qrmci/Runners.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <atomic>
#include <csignal>
#include <expected>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>

namespace {

/*
 * The terminationRequested is a static atomic
 * boolean. This static variable is shared across the application and can be
 * checked in the main loop to determine if the application should terminate
 * gracefully. When a termination signal (SIGINT or SIGTERM) is received, the
 * signalHandler function sets this flag to true.
 */
constinit std::atomic<bool> terminationRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

/*
 * Signal handler for SIGINT and SIGTERM signals.
 * When a termination signal is received, this function sets the termination
 * flag to true, which can be checked in the main loop to terminate the
 * application gracefully.
 */
void signalHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    terminationRequested.store(true, std::memory_order_relaxed);
  }
}
} // namespace

int main() {

  // Register and kill the application when receiving SIGINT or SIGTERM signals
  // This allows the application to terminate gracefully when the user presses
  // Ctrl+C or when the system sends a termination signal.
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Initialize the configuration and logger for the QRM&CI daemon
  const auto loadedConfig = mqss::qrmci::loadConfig();
  if (!loadedConfig) {
    spdlog::error("Failed to load configuration: {}",
                  loadedConfig.error().detail);
    return 1;
  }
  const mqss::qrmci::Config &config = *loadedConfig;
  spdlog::set_default_logger(mqss::qrmci::makeLogger(config.common.daemonLogger,
                                                     config.common.daemonLog));

  // Initialize the communication handler for RabbitMQ with ProtoJson
  // serialization. This handler will be used to receive quantum tasks and send
  // results back.
  auto communicationHandler =
      mqss::qrmci::CommunicationHandler(config.common.connection);

  // Initialize the scheduler with a priority-based scheduling policy. The
  // scheduler will manage the execution order of quantum tasks for the
  // submitter based on their priority.
  mqss::Scheduler<mqss::QuantumTask> scheduler(
      mqss::SchedulingPolicy::PriorityBased);

  // Initialize the submitter to connect to the QDMI backend using the provided
  // driver name, device name, device ID, and authentication token. The
  // submitter will be used to submit quantum tasks to the backend for
  // execution.
  auto submitter = mqss::submitter::Submitter(
      config.submitter.qdmiDriverName, config.submitter.qdmiDeviceName,
      config.submitter.qdmiDeviceName, config.submitter.qdmiClientToken);

  mqss::mqssci::MQSSCompiler compiler;

  mqss::qrmci::BackendRegistry availableBackends(
      config.submitter.backendRegistry.entryTimeToLive,
      config.submitter.backendStatusPublishInterval);
  availableBackends.insertOrRefresh(mqss::qrmci::BackendWrapper(submitter));

  spdlog::info("Available backends:");
  availableBackends.forEachBackend(
      [](const std::string &name, const mqss::qrmci::BackendWrapper &backend) {
        spdlog::info("Backend: {}, Qubits: {}, Status: {}", name,
                     backend.getNumQubits(),
                     static_cast<int>(backend.getStatus()));
      });

  const auto trySend = [&communicationHandler](const auto &message,
                                               const std::string &queueName) {
    if (auto sent = communicationHandler.send(message, queueName); !sent) {
      spdlog::warn("{}", sent.error().detail);
    }
  };

  spdlog::info("Starting MQSS QRM&CI Daemon.");

  // Main loop to process incoming quantum tasks
  while (!terminationRequested.load(std::memory_order_relaxed)) {
    // Refresh the backend registry by claiming a publish slot and inserting
    // or refreshing the backend information. This ensures that the backend
    // registry is up-to-date and available for task selection.
    if (availableBackends.claimPublishSlot()) {
      availableBackends.insertOrRefresh(mqss::qrmci::BackendWrapper(submitter));
    }
    availableBackends.expire();

    // The receive below waits for a bounded time rather than indefinitely:
    // the registry entry for this daemon's own device carries a
    // time-to-live, so the refresh above has to keep getting turns even
    // while no task arrives.
    auto nextTask = communicationHandler.receive<mqss::QuantumTask>(
        config.common.qrmciQueue, config.selector.taskReceiveTimeout,
        terminationRequested);
    if (!nextTask) {
      spdlog::warn("Could not read from queue '{}': {}",
                   config.common.qrmciQueue, nextTask.error().detail);
    } else if (nextTask->has_value()) {
      mqss::QuantumTask task = std::move(**nextTask);
      spdlog::info("Received task: {}. Selecting backend.", task.task_id());

      auto backendSelectedAndCompiled =
          mqss::qrmci::chooseBackend(task, availableBackends)
              .and_then([&task, &availableBackends,
                         &compiler](const std::string &chosenBackend)
                            -> std::expected<void, mqss::qrmci::Error> {
                task.set_scheduled_qpu(chosenBackend);
                spdlog::info("Selected backend: {}. Compiling task: {}",
                             task.scheduled_qpu(), task.task_id());
                const auto *backendInfo = availableBackends.find(chosenBackend);
                if (backendInfo == nullptr) {
                  // Only reachable if the backend expired between being
                  // chosen and being looked up.
                  return std::unexpected(mqss::qrmci::Error{
                      .kind = mqss::qrmci::Error::Kind::NoBackendAvailable,
                      .detail = "Backend '" + chosenBackend +
                                "' is no longer registered."});
                }
                return mqss::qrmci::compileQuantumTask(task, *backendInfo,
                                                       compiler);
              })
              .and_then([&task, &scheduler]() {
                spdlog::info("Task {} compiled. Scheduling for execution.",
                             task.task_id());
                scheduler.scheduleJob(task);

                spdlog::debug("Task {} scheduled with priority {}. total "
                              "scheduled jobs: {}",
                              task.task_id(), task.priority(),
                              scheduler.getJobCount());
                return std::expected<void, mqss::qrmci::Error>{};
              });

      if (!backendSelectedAndCompiled) {
        const auto &failure = backendSelectedAndCompiled.error();
        trySend(mqss::qrmci::cancelQuantumTask(task, failure.detail),
                task.result_destination());
        spdlog::warn("Task {} could not be scheduled. [{}]: {}", task.task_id(),
                     mqss::qrmci::toString(failure.kind), failure.detail);
      }
    }

    // Check if there are any ready jobs to execute and submit them to the
    // backend.
    while (auto nextJob = scheduler.getNextReadyJob()) {
      spdlog::info("Next job to execute: {} with priority {}.",
                   nextJob->task_id(), nextJob->priority());

      auto executionResult =
          mqss::qrmci::executeQuantumTask(*nextJob, submitter);

      if (executionResult) {
        trySend(executionResult.value(), nextJob->result_destination());
        spdlog::info(
            "Job Executed successfully. Results for task {} sent to '{}'.",
            nextJob->task_id(), nextJob->result_destination());
      } else { // If execution fails, send a cancellation result back
        const auto &failure = executionResult.error();
        trySend(mqss::qrmci::cancelQuantumTask(*nextJob, failure.detail),
                nextJob->result_destination());
        spdlog::warn("Execution failed for task {} [{}]: {}",
                     nextJob->task_id(), mqss::qrmci::toString(failure.kind),
                     failure.detail);
      }
    }

    std::this_thread::sleep_for(config.common.stagePollInterval);
  }

  spdlog::info("Shutting down MQSS QRM&CI Daemon.");

  return 0;
}
