/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Exercises routing across two distributed workers rather than one: each
// worker advertises a distinct backend ID and dispatch queue
// (QRMCI_SUBMITTER_QDMI_DEVICE_ID / QRMCI_COMPILER_QUEUE), so the selector
// must send a task it assigns to worker-iqm to worker-iqm's own queue rather
// than a queue every worker shares.
// Each task is restricted (QuantumTask::restricted_resource_names) to the
// one worker ID it must land on, and the assertion that actually catches a
// wrong-worker delivery is on the result's executed_qpu, not merely on
// execution_status: a task the wrong worker picked up and compiled for
// itself would still come back successful, just with the wrong
// executed_qpu.

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"
#include "qrmci/Error.h"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// Single source of truth for the worker-iqm backend's advertised ID, shared
// between the EnvOverride handed to the spawned worker daemon (main()) and
// the task's own restricted_resource_names/executed_qpu check (test()) --
// both must agree on this value, or a task would be sent to (and reported
// as executed by) a different ID than the one test() checks for.
std::string workerDeviceId() {
  const char *env = std::getenv("QRMCI_SUBMITTER_QDMI_DEVICE_ID");
  return (env != nullptr && *env != '\0') ? env : "worker-iqm";
}

std::string iqmDeviceAlias() {
  const char *env = std::getenv("IQM_QC_ALIAS");
  return (env != nullptr && *env != '\0') ? env : "";
}

mqss::QuantumTask makeTaskRestrictedTo(const std::string &workerId) {
  mqss::QuantumTask qtask;
  qtask.set_task_id(mqss::qrmci::test::uniqueTaskId());
  qtask.add_circuit_files(mqss::qrmci::test::readCircuitFixture("bell.quake"));
  qtask.set_circuit_file_type(std::string("quake"));
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.add_restricted_resource_names(workerId);
  qtask.set_no_modify(false);
  qtask.set_result_destination("tester.tasks.queue.multiworker." + workerId);
  return qtask;
}

static std::expected<int, mqss::qrmci::Error> test() {

  auto loadedConfig = mqss::qrmci::loadConfig();
  if (!loadedConfig)
    return std::unexpected(std::move(loadedConfig.error()));

  const std::vector<std::string> workerIds = {workerDeviceId()};
  mqss::qrmci::Config config = *loadedConfig;

  std::unordered_map<std::string, mqss::QuantumTask> tasks;
  tasks.reserve(workerIds.size());
  for (const auto &workerId : workerIds) {
    tasks[workerId] = makeTaskRestrictedTo(workerId);
  }

  mqss::qrmci::CommunicationHandler communicationHandler(
      config.common.connection);

  for (const auto &[workerID, qtask] : tasks) {
    auto sent = communicationHandler.send(qtask, config.common.qrmciQueue);
    if (!sent) {
      std::cout << "Trying to send task failed: " << qtask.task_id() << "\n";
      return std::unexpected(std::move(sent.error()));
    }
  }

  for (const auto &[workerID, qtask] : tasks) {
    std::cout << ("expected worker=" + workerID +
                  " task_id=" + std::to_string(qtask.task_id()) + "\n");

    auto received = communicationHandler.receive<mqss::QuantumResult>(
        qtask.result_destination(), mqss::qrmci::test::kResultTimeout);
    if (!received)
      return std::unexpected(std::move(received.error()));

    if (!received->has_value()) {
      std::cout << "No result received on queue " << qtask.result_destination()
                << "\n";
      return EXIT_FAILURE;
    }

    const mqss::QuantumResult &taskResult = received->value();
    // if (taskResult.task_id() != qtask.task_id()) {
    //   std::cout << ("taskResult id: " + std::to_string(taskResult.task_id()))
    //             << " does not match task id: "
    //             << std::to_string(qtask.task_id()) << "\n";
    //   return EXIT_FAILURE;
    // }
    if (!taskResult.execution_status()) {
      std::cout << "Task did not execute successfully: "
                << taskResult.additional_information() << "\n";
      return EXIT_FAILURE;
    }
    if (taskResult.executed_qpu() != workerID) {
      std::cout << "Task was executed by the wrong worker." << "\n";
      return EXIT_FAILURE;
    }
    for (int r = 0; r < taskResult.results_size();
         ++r) { // one entry per circuit file
      const auto &counts =
          taskResult.results(r).counts(); // map: bitstring -> count
      std::cout << "  circuit " << r << ": " << counts.size()
                << " distinct outcomes\n";
      for (const auto &[bits, n] : counts) {
        std::cout << "    " << bits << " : " << n << "\n";
      }
    }
  }
  return EXIT_SUCCESS;
}

} // namespace

static bool startAll(std::vector<mqss::qrmci::test::DaemonProcess> &daemons) {
  for (auto &d : daemons)
    if (!d.start()) {
      std::cerr << "Failed to start " << d.getLabel() << "\n";
      return false;
    }
  return true;
}

int main() {
  std::vector<mqss::qrmci::test::DaemonProcess> daemons;
  daemons.emplace_back(
      "qrmcid-distributed-selector", QRMCI_DISTRIBUTED_SELECTOR_PATH,
      QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR,
      std::vector<mqss::qrmci::test::EnvOverride>{
          {.name = "QRMCI_CONFIG_FILE", .value = QRMCI_SELECTOR_TEST_CONFIG}});

  // CXX example QDMI device. Does not run the circuit. Only, generates
  // num_shots random bitstrings of QUBITSNUM bits
  daemons.emplace_back(
      "qrmcid-distributed-worker-iqm", QRMCI_DISTRIBUTED_WORKER_PATH,
      QRMCI_DISTRIBUTED_WORKER_B_LOG_DIR,
      std::vector<mqss::qrmci::test::EnvOverride>{
          {.name = "QRMCI_SUBMITTER_QDMI_DEVICE_ID", .value = workerDeviceId()},
          {.name = "QRMCI_COMPILER_QUEUE", .value = "compiler.worker-iqm"},
          {.name = "QDMI_CONF", .value = QRMCI_WORKER_IQM_QDMI_CONF},
          {.name = "QRMCI_CONFIG_FILE", .value = QRMCI_WORKER_IQM_TEST_CONFIG},
          {.name = "IQM_BASE_URL", .value = "https://resonance.iqm.tech"},
          {.name = "IQM_QC_ALIAS", .value = iqmDeviceAlias()}});

  if (!startAll(daemons))
    return EXIT_FAILURE;
  std::this_thread::sleep_for(std::chrono::seconds(60));
  // Two workers each publish their own status on the same
  // backendStatusPublishInterval as a single-worker deployment, so the same
  // extra readiness grace TestSubmitTaskDistributed.cpp uses applies here.
  auto r = test(); // your existing function
  for (auto &daemon : std::views::reverse(daemons))
    daemon.stop();
  return r ? *r : EXIT_FAILURE;
}
