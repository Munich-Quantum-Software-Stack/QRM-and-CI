/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/protocol/ProtoProtocol.hpp"
#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"
#include "qrmci/Error.h"
#include "qrmci/Logger.h"
#include "qrmci/Runners.h"

#include <atomic>
#include <csignal>
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

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  const auto loadedConfig = mqss::qrmci::loadConfig();
  if (!loadedConfig) {
    spdlog::error("Failed to load configuration: {}",
                  loadedConfig.error().detail);
    return 1;
  }
  const mqss::qrmci::Config &config = *loadedConfig;
  spdlog::set_default_logger(mqss::qrmci::makeLogger(config.common.daemonLogger,
                                                     config.common.daemonLog));

  auto communicationHandler =
      mqss::qrmci::CommunicationHandler(config.common.connection);

  // This process holds no device, so its registry is filled entirely from the
  // status workers publish, and emptied by the time-to-live when one stops
  // publishing.
  mqss::qrmci::BackendRegistry availableBackends(
      config.selector.backendRegistry.entryTimeToLive,
      mqss::qrmci::BackendRegistry::DefaultPublishInterval);

  const auto trySend = [&communicationHandler](const auto &message,
                                               const std::string &queueName) {
    if (auto sent = communicationHandler.send(message, queueName); !sent) {
      spdlog::warn("{}", sent.error().detail);
    }
  };

  spdlog::info("Starting MQSS QRM&CI Distributed Backend Selector.");

  while (!terminationRequested.load(std::memory_order_relaxed)) {
    // Absorb one published backend status, if one is waiting, and drop any
    // entry nobody has refreshed in time.
    auto backendStatus = communicationHandler.receive<mqss::Backend>(
        config.selector.backendStatusQueue,
        config.selector.backendStatusReceiveTimeout, terminationRequested);
    if (!backendStatus) {
      spdlog::warn("Could not read from queue '{}': {}",
                   config.selector.backendStatusQueue,
                   backendStatus.error().detail);
    } else if (backendStatus->has_value()) {
      auto backend = mqss::qrmci::BackendWrapper(**backendStatus);
      spdlog::debug("Updated backend status for '{}' (qubits: {}, status: {}).",
                    backend.getName(), backend.getNumQubits(),
                    static_cast<int>(backend.getStatus()));
      availableBackends.insertOrRefresh(std::move(backend));
    }
    availableBackends.expire();

    auto nextTask = communicationHandler.receive<mqss::QuantumTask>(
        config.common.qrmciQueue, config.selector.taskReceiveTimeout,
        terminationRequested);
    if (!nextTask) {
      spdlog::warn("Could not read from queue '{}': {}",
                   config.common.qrmciQueue, nextTask.error().detail);
    } else if (nextTask->has_value()) {
      mqss::QuantumTask task = std::move(**nextTask);
      spdlog::info("Received task: {}. Selecting backend.", task.task_id());

      auto chosenBackend = mqss::qrmci::chooseBackend(task, availableBackends);
      if (chosenBackend) {
        task.set_scheduled_qpu(*chosenBackend);
        spdlog::info("Selected backend: {} for task {}. Forwarding to '{}'.",
                     *chosenBackend, task.task_id(), config.compiler.queue);
        trySend(task, config.compiler.queue);
      } else {
        const auto &failure = chosenBackend.error();
        trySend(mqss::qrmci::cancelQuantumTask(task, failure.detail),
                task.result_destination());
        spdlog::warn("Task {} could not be assigned a backend [{}]: {}",
                     task.task_id(), mqss::qrmci::toString(failure.kind),
                     failure.detail);
      }
    }

    std::this_thread::sleep_for(config.common.stagePollInterval);
  }

  spdlog::info("Shutting down MQSS QRM&CI Distributed Backend Selector.");

  return 0;
}
