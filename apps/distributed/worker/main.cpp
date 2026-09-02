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
#include <cstdint>
#include <expected>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

  mqss::Scheduler<mqss::QuantumTask> scheduler(
      mqss::SchedulingPolicy::PriorityBased);

  auto submitter = mqss::submitter::Submitter(
      config.submitter.qdmiDriverName, config.submitter.qdmiDeviceName,
      config.submitter.qdmiDeviceName, config.submitter.qdmiClientToken);

  mqss::mqssci::MQSSCompiler compiler;

  // Backend selection happens in another process, so this worker publishes its
  // own device's status for the selector to absorb, and looks tasks up in the
  // same registry when it compiles them.
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

  spdlog::info("Starting MQSS QRM&CI Distributed Worker.");

  while (!terminationRequested.load(std::memory_order_relaxed)) {
    if (availableBackends.claimPublishSlot()) {
      auto device = mqss::qrmci::BackendWrapper(submitter);
      trySend(device.toBackend(), config.selector.backendStatusQueue);
      availableBackends.insertOrRefresh(std::move(device));
    }
    availableBackends.expire();

    auto nextTask = communicationHandler.receive<mqss::QuantumTask>(
        config.compiler.queue, config.compiler.taskReceiveTimeout,
        terminationRequested);
    if (!nextTask) {
      spdlog::warn("Could not read from queue '{}': {}", config.compiler.queue,
                   nextTask.error().detail);
    } else if (nextTask->has_value()) {
      mqss::QuantumTask task = std::move(**nextTask);
      spdlog::info("Received task: {}. Compiling task.", task.task_id());

      std::expected<void, mqss::qrmci::Error> compiled;
      const auto *backendInfo = availableBackends.find(task.scheduled_qpu());
      if (backendInfo == nullptr) {
        compiled = std::unexpected(mqss::qrmci::Error{
            .kind = mqss::qrmci::Error::Kind::NoBackendAvailable,
            .detail = "Backend '" + task.scheduled_qpu() +
                      "' is not registered with this process."});
      } else {
        compiled =
            mqss::qrmci::compileQuantumTask(task, *backendInfo, compiler)
                .and_then([&task, &scheduler]()
                              -> std::expected<void, mqss::qrmci::Error> {
                  spdlog::info("Task {} compiled. Scheduling for execution.",
                               task.task_id());
                  scheduler.scheduleJob(task);
                  return {};
                });
      }

      if (!compiled) {
        const auto &failure = compiled.error();
        trySend(mqss::qrmci::cancelQuantumTask(task, failure.detail),
                task.result_destination());
        spdlog::warn("Task {} could not be scheduled. [{}]: {}", task.task_id(),
                     mqss::qrmci::toString(failure.kind), failure.detail);
      }
    }

    // Submit every ready job before collecting any result, so several jobs are
    // in flight on the device at once instead of one at a time.
    std::vector<std::pair<mqss::QuantumTask, std::uint32_t>> submittedJobs;
    while (auto nextJob = scheduler.getNextReadyJob()) {
      spdlog::info("Submitting job: {} with priority {}.", nextJob->task_id(),
                   nextJob->priority());
      auto jobId = mqss::qrmci::submitQuantumTask(*nextJob, submitter);
      if (jobId) {
        submittedJobs.emplace_back(*nextJob, *jobId);
      } else {
        const auto &failure = jobId.error();
        trySend(mqss::qrmci::cancelQuantumTask(*nextJob, failure.detail),
                nextJob->result_destination());
        spdlog::warn("Submission failed for task {} [{}]: {}",
                     nextJob->task_id(), mqss::qrmci::toString(failure.kind),
                     failure.detail);
      }
    }

    for (auto &[job, jobId] : submittedJobs) {
      auto executionResult =
          mqss::qrmci::collectQuantumResult(job, jobId, submitter);
      if (executionResult) {
        trySend(executionResult.value(), job.result_destination());
        spdlog::info(
            "Job Executed successfully. Results for task {} sent to '{}'.",
            job.task_id(), job.result_destination());
      } else {
        const auto &failure = executionResult.error();
        trySend(mqss::qrmci::cancelQuantumTask(job, failure.detail),
                job.result_destination());
        spdlog::warn("Execution failed for task {} [{}]: {}", job.task_id(),
                     mqss::qrmci::toString(failure.kind), failure.detail);
      }
    }

    std::this_thread::sleep_for(config.common.stagePollInterval);
  }

  spdlog::info("Shutting down MQSS QRM&CI Distributed Worker.");

  return 0;
}
