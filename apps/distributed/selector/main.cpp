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
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Daemon.h"
#include "qrmci/DaemonMessaging.h"
#include "qrmci/Error.h"
#include "qrmci/Runners.h"

#include <cstdlib>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>

int main() {
  using namespace mqss::qrmci;

  const auto loadedConfig = initializeDaemon();
  if (!loadedConfig) {
    spdlog::error("Failed to load configuration: {}",
                  loadedConfig.error().detail);
    return EXIT_FAILURE;
  }
  const Config &config = *loadedConfig;

  CommunicationHandler comms(config.common.connection);

  // Workers populate the registry with status updates; stale entries expire.
  BackendRegistry availableBackends(
      config.selector.backendRegistry.entryTimeToLive);

  spdlog::info("Starting MQSS QRM&CI Distributed Backend Selector.");

  while (!terminationRequested.load(std::memory_order_relaxed)) {
    drainQueue<mqss::Backend>(
        comms, config.selector.backendStatusQueue, terminationRequested,
        [&availableBackends](const mqss::Backend &status) {
          auto backend = BackendWrapper::fromPublishedStatus(status);
          if (!backend) {
            spdlog::warn("Rejected published backend status: {}",
                         backend.error().detail);
            return;
          }
          spdlog::debug(
              "Updated backend status for '{}' (qubits: {}, status: {}).",
              backend->getName(), backend->getNumQubits(),
              backendStatusLabel(backend->getStatus()));
          const std::string name = backend->getName();
          const std::string queueName = backend->getQueueName();
          if (!availableBackends.insertOrRefresh(std::move(*backend))) {
            spdlog::warn("Rejected backend status for '{}': dispatch queue "
                         "'{}' conflicts with its already-registered queue.",
                         name, queueName);
          }
        });
    availableBackends.expire();

    const bool receivedTasks = drainQueue<mqss::QuantumTask>(
        comms, config.common.qrmciQueue, terminationRequested,
        [&](mqss::QuantumTask task) {
          spdlog::info("Received task: {}. Selecting backend.", task.task_id());

          auto backendName = chooseBackend(task, availableBackends,
                                           config.selector.selectionPolicy);
          if (!backendName) {
            sendFailure(comms, task, backendName.error(),
                        "could not be assigned a backend.");
            return;
          }

          // Resolve the selected backend's dispatch queue from the registry.
          const auto *backend = availableBackends.find(*backendName);
          if (backend == nullptr) {
            sendFailure(comms, task,
                        Error{Error::Kind::NoBackendAvailable,
                              "Backend '" + *backendName +
                                  "' is no longer registered."},
                        "could not be assigned a backend.");
            return;
          }

          task.set_scheduled_qpu(*backendName);
          spdlog::info("Selected backend: {} for task {}. Forwarding to '{}'.",
                       *backendName, task.task_id(), backend->getQueueName());
          send(comms, task, backend->getQueueName());
        });

    if (!receivedTasks) {
      std::this_thread::sleep_for(config.common.stagePollInterval);
    }
  }

  spdlog::info("Shutting down MQSS QRM&CI Distributed Backend Selector.");

  return EXIT_SUCCESS;
}
