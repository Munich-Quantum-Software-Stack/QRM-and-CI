/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/protocol/ProtoProtocol.hpp"
#include "qrmci/BackendRegistry.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"
#include "qrmci/Daemon.h"
#include "qrmci/DaemonMessaging.h"
#include "qrmci/Runners.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <cstdlib>
#include <mqss/submitter/Device.h>
#include <spdlog/spdlog.h>
#include <thread>

int main() {
  using namespace mqss::qrmci;

  const auto loadedConfig = initializeDaemon();
  if (!loadedConfig) {
    spdlog::error("Failed to load configuration: {}",
                  loadedConfig.error().detail);
    return EXIT_FAILURE;
  }
  const Config &config = *loadedConfig;

  // Receives quantum tasks and sends results back, over RabbitMQ with
  // ProtoJson serialization.
  CommunicationHandler comms(config.common.connection);

  // The scheduler manages the execution order of quantum tasks for the
  // submitter based on their priority.
  TaskScheduler scheduler(mqss::scheduler::SchedulingPolicy::PriorityBased);

  auto openedDevice = openConfiguredDevice(config.submitter);
  if (!openedDevice) {
    spdlog::error("Could not open the QDMI device: {}",
                  openedDevice.error().detail);
    return EXIT_FAILURE;
  }
  const mqss::submitter::Device &device = *openedDevice;

  mqss::mqssci::MQSSCompiler compiler;

  BackendRegistry availableBackends(
      config.submitter.backendRegistry.entryTimeToLive);
  PublicationThrottle statusPublishThrottle(
      config.submitter.backendStatusPublishInterval);

  // The daemon has nothing to offer if it cannot read its own device once.
  if (auto registered = registerOwnDevice(device, availableBackends);
      !registered) {
    spdlog::error("Could not read the QDMI device: [{}] {}",
                  toString(registered.error().kind), registered.error().detail);
    return EXIT_FAILURE;
  }
  logRegisteredBackends(availableBackends);

  spdlog::info("Starting MQSS QRM&CI Daemon.");

  while (!terminationRequested.load(std::memory_order_relaxed)) {
    refreshOwnDeviceStatus(device, availableBackends, statusPublishThrottle);
    availableBackends.expire();

    const bool receivedTasks = drainQueue<mqss::QuantumTask>(
        comms, config.common.qrmciQueue, terminationRequested,
        [&](mqss::QuantumTask task) {
          spdlog::info("Received task: {}. Selecting backend.", task.task_id());
          if (auto prepared = selectCompileAndSchedule(
                  task, availableBackends, compiler, scheduler,
                  config.selector.selectionPolicy);
              !prepared) {
            sendFailure(comms, task, prepared.error(),
                        "could not be scheduled.");
          }
        });

    bool executedTasks = false;
    while (auto nextTask = scheduler.getNextReadyTask()) {
      executedTasks = true;
      spdlog::info("Next task to execute: {} with priority {}.",
                   nextTask->task_id(), nextTask->priority());

      auto executionResult =
          executeQuantumTask(*nextTask, device, config.submitter.jobWaitTimeout,
                             terminationRequested);
      if (executionResult) {
        sendResult(comms, *nextTask, *executionResult);
        continue;
      }
      if (executionResult.error().kind == Error::Kind::ShutdownRequested) {
        spdlog::info("Shutdown requested; abandoning task {} in flight.",
                     nextTask->task_id());
        break;
      }
      sendFailure(comms, *nextTask, executionResult.error(),
                  "could not be executed.");
    }

    // Avoid busy-waiting when no tasks were received or executed.
    if (!receivedTasks && !executedTasks) {
      std::this_thread::sleep_for(config.common.stagePollInterval);
    }
  }

  spdlog::info("Shutting down MQSS QRM&CI Daemon.");

  return EXIT_SUCCESS;
}
