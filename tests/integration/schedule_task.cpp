/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

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

// Reuse the same MLIR circuit as submit_task.cpp; this test is
// about scheduling behavior, not compiler coverage, so every task can share
// the same circuit body.
const std::string sampleCircuit = R"(
func.func @__nvqpp__mlirgen__testILm2EE() attributes {"cudaq-entrypoint", "cudaq-kernel"} {
  %0 = quake.alloca !quake.veq<2>
  %1 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x %1 : (!quake.ref) -> ()
  %2 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %3 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%2] %3 : (!quake.ref, !quake.ref) -> ()
  %measOut = quake.mz %0 : (!quake.veq<2>) -> !cc.stdvec<!quake.measure>
  return
}
)";

struct TaskSpec {
  int32_t task_id;
  int32_t priority;
};

// Submitted out of priority order: if the daemon's scheduler
// has more than one of these queued at once, a PriorityBased policy should
// let 103 (priority 10) and 110 (priority 9) reach the submitter ahead of
// others, even though the others might be sent first.
const std::vector<TaskSpec> nTasks = {
    {101, 1},
    {102, 5},
    {103, 10},
    {104, 3},
    {105, 7},
    {106, 2},
    {107, 8},
    {108, 4},
    {109, 6},
    {110, 9},
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

  // Initialize the configuration from command-line arguments
  mqss::qrmci::initConfig(mqss::qrmci::loadConfig(argc, argv));
  auto config = mqss::qrmci::getConfig();

  // Declare a communication handler to send tasks and receive results.
  mqss::qrmci::CommunicationHandler<mqss::RabbitMqSimple, mqss::ProtoJson>
      communication_handler(config.rabbitmq);

  // To mark the priority of each task, so we can check the arrival order later.
  std::unordered_map<int32_t, int32_t> priority_assignment;
  for (const auto &spec : nTasks) {
    priority_assignment[spec.task_id] = spec.priority;
  }

  // Send every task back to back, with nothing to make the daemon wait
  // between them, so several land in its scheduler queue together.
  std::cout << "Submitting " << nTasks.size()
            << " tasks (out of priority order) to queue: "
            << config.queues.qrmci << std::endl;
  for (const auto &spec : nTasks) {
    mqss::QuantumTask qtask;
    qtask.set_task_id(spec.task_id);
    qtask.add_circuit_files(sampleCircuit);
    qtask.set_circuit_file_type(std::string("mlir"));
    qtask.set_n_shots(100);
    qtask.set_optimisation_level(1);
    qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
    qtask.set_no_modify(false);
    qtask.set_priority(spec.priority);
    qtask.set_result_destination(nResultQueue);

    communication_handler.sendQuantumTask(qtask, config.queues.qrmci);
    std::cout << "  sent task_id=" << spec.task_id
              << " priority=" << spec.priority << std::endl;
  }

  // Collect one result per submitted task, in whatever order they arrive.
  std::vector<int32_t> arrival_order;
  std::cout << "Waiting for " << nTasks.size()
            << " results on queue: " << nResultQueue << std::endl;
  while (arrival_order.size() < nTasks.size()) {
    auto opt_result = communication_handler.getNextQuantumResult(
        nResultQueue, std::chrono::milliseconds(0));

    if (!opt_result.has_value()) {
      std::cerr << "No result received; expected " << nTasks.size()
                << " but only got " << arrival_order.size() << "."
                << std::endl;
      return 1;
    }

    mqss::QuantumResult result = opt_result.value();
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
}
