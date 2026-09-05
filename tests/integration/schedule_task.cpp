/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "IntegrationTestHelpers.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// An example: submits several tasks with different priorities
// (before the daemon has a chance to drain any of them), then checks
// that every task comes back with a matching result. This example is to
// check the daemon's mqss::Scheduler<mqss::QuantumTask> (constructed with
// SchedulingPolicy::PriorityBased in apps/standalone/main.cpp).
//
// Requires a running qrmci_aio_daemon and a reachable RabbitMQ, same as
// submit_task.cpp.

namespace {

struct TaskSpec {
  int32_t task_id;
  int32_t priority;
};

// Submitted out of priority order: if the daemon's scheduler
// has more than one of these queued at once, a PriorityBased policy should
// let 103 (priority 10) and 110 (priority 9) reach the submitter ahead of
// others, even though the others might be sent first.
const std::vector<TaskSpec> nTasks = {
    {101, 1}, {102, 5}, {103, 10}, {104, 3}, {105, 7},
    {106, 2}, {107, 8}, {108, 4},  {109, 6}, {110, 9},
};

const std::string nResultQueue = "schedule_task.results.queue";

} // namespace

/*
 * Main function
 *
 * Playing as a client, this program submits several tasks
 * with different priorities to the daemon's scheduler,
 * then waits for results and checks that every task comes
 * back with a matching result.
 */
int main(int argc, char **argv) {

  auto config = mqss::qrmci::loadConfig();
  if (!config) {
    std::cerr << config.error().detail << "\n";
    return 1;
  }

  mqss::qrmci::CommunicationHandler communicationHandler(
      config->common.connection);

  // To mark the priority of each task, so we can check the arrival order later.
  std::unordered_map<int32_t, int32_t> priority_assignment;
  for (const auto &spec : nTasks) {
    priority_assignment[spec.task_id] = spec.priority;
  }

  // Send every task back to back, with nothing to make the daemon wait
  // between them, so several land in its scheduler queue together.
  std::cout << "Submitting " << nTasks.size()
            << " tasks (out of priority order) to queue: "
            << config->common.qrmciQueue << std::endl;
  for (const auto &spec : nTasks) {
    mqss::QuantumTask qtask;
    qtask.set_task_id(spec.task_id);
    qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
    qtask.set_circuit_file_type(std::string("quake"));
    qtask.set_n_shots(100);
    qtask.set_optimisation_level(1);
    qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
    qtask.set_no_modify(false);
    qtask.set_priority(spec.priority);
    qtask.set_result_destination(nResultQueue);

    if (auto sent = communicationHandler.send(qtask, config->common.qrmciQueue);
        !sent) {
      std::cerr << sent.error().detail << "\n";
      return 1;
    }
    std::cout << "  sent task_id=" << spec.task_id
              << " priority=" << spec.priority << std::endl;
  }

  // Collect one result per submitted task, in whatever order they arrive.
  std::vector<int32_t> arrival_order;
  std::cout << "Waiting for " << nTasks.size()
            << " results on queue: " << nResultQueue << std::endl;
  while (arrival_order.size() < nTasks.size()) {
    auto received = communicationHandler.receive<mqss::QuantumResult>(
        nResultQueue, std::chrono::milliseconds(0));
    if (!received.has_value()) {
      std::cerr << received.error().detail << "\n";
      return 1;
    }
    const auto &optResult = *received;

    if (!optResult.has_value()) {
      std::cerr << "No result received; expected " << nTasks.size()
                << " but only got " << arrival_order.size() << "." << std::endl;
      return 1;
    }

    const auto &result = optResult.value();
    if (!priority_assignment.contains(result.task_id())) {
      std::cerr << "Received result for unexpected task_id: "
                << result.task_id() << std::endl;
      return 1;
    }

    arrival_order.push_back(result.task_id());
    std::cout << "  received result for task_id=" << result.task_id()
              << " (priority=" << priority_assignment[result.task_id()] << ")"
              << std::endl;
  }

  // Every submitted task must come back exactly once; this is the part of
  // the test that must always hold, independent of scheduling/timing.
  std::vector<int32_t> expected_ids;
  for (const auto &spec : nTasks) {
    expected_ids.push_back(spec.task_id);
  }
  std::vector<int32_t> sorted_arrival = arrival_order;
  std::vector<int32_t> sorted_expected = expected_ids;
  std::sort(sorted_arrival.begin(), sorted_arrival.end());
  std::sort(sorted_expected.begin(), sorted_expected.end());
  if (sorted_arrival != sorted_expected) {
    std::cerr << "Result set does not match the submitted task set."
              << std::endl;
    return 1;
  }

  // Whether arrival order actually reflects priority is timing-dependent
  // (it only shows up if more than one task was genuinely queued at once
  // inside the daemon's scheduler), so this is reported, not asserted.
  std::vector<int32_t> priority_sorted_ids = expected_ids;
  std::sort(priority_sorted_ids.begin(), priority_sorted_ids.end(),
            [&](int32_t a, int32_t b) {
              return priority_assignment[a] > priority_assignment[b];
            });

  std::cout << "\nSubmission order:      ";
  for (auto id : expected_ids) {
    std::cout << id << "(p" << priority_assignment[id] << ") ";
  }
  std::cout << "\nPriority order:        ";
  for (auto id : priority_sorted_ids) {
    std::cout << id << "(p" << priority_assignment[id] << ") ";
  }
  std::cout << "\nActual arrival order:  ";
  for (auto id : arrival_order) {
    std::cout << id << "(p" << priority_assignment[id] << ") ";
  }
  std::cout << std::endl;

  if (arrival_order == priority_sorted_ids) {
    std::cout << "\nArrival order matches priority order: scheduler "
                 "reordered these tasks."
              << std::endl;
  } else {
    std::cout << "\nArrival order does not match strict priority order "
                 "(expected if the daemon drained tasks faster than they "
                 "arrived, so few/none were ever queued together)."
              << std::endl;
  }

  std::cout << "\nAll " << nTasks.size()
            << " tasks submitted and returned correctly." << std::endl;
  return 0;

  // TODO: fix this test to check that the arrival order is actually
  // priority-based, not just that all tasks came back. The problem is that the
  // daemon may drain tasks faster than they arrive, so the scheduler never has
  // more than one task queued at once, and the arrival order will reflect
  // submission order instead of priority order.
}
