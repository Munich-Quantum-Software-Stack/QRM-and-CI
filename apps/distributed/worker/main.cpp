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
#include <mqss/submitter/Job.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <utility>
#include <vector>

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

  TaskScheduler scheduler(mqss::scheduler::SchedulingPolicy::PriorityBased);

  auto openedDevice = openConfiguredDevice(config.submitter);
  if (!openedDevice) {
    spdlog::error("Could not open the QDMI device: {}",
                  openedDevice.error().detail);
    return EXIT_FAILURE;
  }
  const mqss::submitter::Device &device = *openedDevice;

  mqss::mqssci::MQSSCompiler compiler;

  // Keep the worker's device status available for local compilation and publish
  // it to the selector.
  BackendRegistry availableBackends(
      config.submitter.backendRegistry.entryTimeToLive);
  PublicationThrottle statusPublishThrottle(
      config.submitter.backendStatusPublishInterval);

  // Register the device before serving tasks, using this worker's compiler
  // queue as the dispatch queue advertised to selectors.
  if (auto registered =
          registerOwnDevice(device, availableBackends, config.compiler.queue);
      !registered) {
    spdlog::error("Could not read the QDMI device: [{}] {}",
                  toString(registered.error().kind), registered.error().detail);
    return EXIT_FAILURE;
  }
  logRegisteredBackends(availableBackends);

  spdlog::info("Starting MQSS QRM&CI Distributed Worker.");

  while (!terminationRequested.load(std::memory_order_relaxed)) {
    if (auto status = refreshOwnDeviceStatus(device, availableBackends,
                                             statusPublishThrottle,
                                             config.compiler.queue)) {
      send(comms, *status, config.selector.backendStatusQueue);
    }
    availableBackends.expire();

    // One task per turn, so the jobs already scheduled below are submitted
    // without waiting for the queue to run dry first.
    const bool receivedTask = receiveNext<mqss::QuantumTask>(
        comms, config.compiler.queue, config.compiler.taskReceiveTimeout,
        terminationRequested, [&](mqss::QuantumTask task) {
          spdlog::info("Received task: {}. Compiling task.", task.task_id());
          if (auto compiled = compileAndSchedule(task, availableBackends,
                                                 compiler, scheduler);
              !compiled) {
            sendFailure(comms, task, compiled.error(),
                        "could not be scheduled.");
          }
        });

    // Submit every ready job before collecting any result, so several jobs are
    // in flight on the device at once instead of one at a time.
    std::vector<std::pair<mqss::QuantumTask, std::vector<mqss::submitter::Job>>>
        submittedJobs;
    while (auto nextTask = scheduler.getNextReadyTask()) {
      spdlog::info("Submitting job: {} with priority {}.", nextTask->task_id(),
                   nextTask->priority());
      auto jobs = submitQuantumTask(*nextTask, device, terminationRequested);
      if (jobs) {
        submittedJobs.emplace_back(std::move(*nextTask), std::move(*jobs));
        continue;
      }
      if (jobs.error().kind == Error::Kind::ShutdownRequested) {
        spdlog::info("Shutdown requested; abandoning task {} before "
                     "submission.",
                     nextTask->task_id());
        break;
      }
      sendFailure(comms, *nextTask, jobs.error(), "could not be submitted.");
    }

    for (auto &[submittedTask, jobs] : submittedJobs) {
      auto executionResult = collectQuantumResult(
          submittedTask, jobs, config.submitter.jobWaitTimeout,
          terminationRequested);
      if (executionResult) {
        sendResult(comms, submittedTask, *executionResult);
        continue;
      }
      if (executionResult.error().kind == Error::Kind::ShutdownRequested) {
        spdlog::info("Shutdown requested; abandoning task {} awaiting "
                     "results.",
                     submittedTask.task_id());
        break;
      }
      sendFailure(comms, submittedTask, executionResult.error(),
                  "could not be executed.");
    }

    if (!receivedTask) {
      std::this_thread::sleep_for(config.common.stagePollInterval);
    }
  }

  spdlog::info("Shutting down MQSS QRM&CI Distributed Worker.");

  return EXIT_SUCCESS;
}
