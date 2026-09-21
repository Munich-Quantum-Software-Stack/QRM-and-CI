/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Submits several tasks with different priorities, before the daemon has a
// chance to drain any of them, then checks that every task comes back with a
// matching result. This exercises the standalone daemon's TaskScheduler,
// constructed with SchedulingPolicy::PriorityBased in
// apps/standalone/main.cpp.
//
// It does not assert that results arrive in priority order, only that they
// all arrive. The daemon can drain tasks faster than this test submits them,
// leaving the scheduler with one task queued at a time, in which case the
// arrival order reflects submission order rather than priority. Asserting on
// order needs a way to hold the daemon back while the tasks pile up.

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct TaskSpec {
  int32_t taskId;
  int32_t priority;
};

// Priorities only here: task ids are assigned per run via
// mqss::qrmci::test::uniqueTaskId(), so a stale leftover result from an
// earlier run on the shared, durable ResultQueue below cannot satisfy this
// run's checks. Submitted out of priority order: if the daemon's scheduler
// has more than one of these queued at once, a PriorityBased policy should
// let the priority-10 and priority-9 tasks reach the submitter ahead of the
// others, even though the others might be sent first.
const std::vector<int32_t> Priorities = {1, 5, 10, 3, 7, 2, 8, 4, 6, 9};

const std::string ResultQueue = "schedule_task.results.queue";

class ScheduleTaskTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto loadedConfig = mqss::qrmci::loadConfig();
    ASSERT_TRUE(loadedConfig) << loadedConfig.error().detail;
    config = *loadedConfig;
  }

  mqss::qrmci::Config config;
};

} // namespace

TEST_F(ScheduleTaskTest, ReturnsEveryTaskExactlyOnce) {
  mqss::qrmci::CommunicationHandler communicationHandler(
      config.common.connection);

  std::vector<TaskSpec> tasks;
  tasks.reserve(Priorities.size());
  for (auto priority : Priorities) {
    tasks.push_back(TaskSpec{mqss::qrmci::test::uniqueTaskId(), priority});
  }

  std::unordered_map<int32_t, int32_t> priorityAssignment;
  for (const auto &spec : tasks) {
    priorityAssignment[spec.taskId] = spec.priority;
  }

  // Send every task back to back, with nothing to make the daemon wait
  // between them, so several land in its scheduler queue together.
  for (const auto &spec : tasks) {
    mqss::QuantumTask qtask;
    qtask.set_task_id(spec.taskId);
    qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
    qtask.set_circuit_file_type(std::string("quake"));
    qtask.set_n_shots(100);
    qtask.set_optimisation_level(1);
    qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
    qtask.set_no_modify(false);
    qtask.set_priority(spec.priority);
    qtask.set_result_destination(ResultQueue);

    auto sent = communicationHandler.send(qtask, config.common.qrmciQueue);
    ASSERT_TRUE(sent) << "Sending task_id=" << spec.taskId << ": "
                      << sent.error().detail;
  }

  // Collect one result per submitted task, in whatever order they arrive.
  std::vector<int32_t> arrivalOrder;
  while (arrivalOrder.size() < tasks.size()) {
    auto received = communicationHandler.receive<mqss::QuantumResult>(
        ResultQueue, mqss::qrmci::test::kPolledResultTimeout);
    ASSERT_TRUE(received) << received.error().detail;
    ASSERT_TRUE(received->has_value())
        << "No result received; expected " << tasks.size() << " but only got "
        << arrivalOrder.size() << ".";

    const auto &result = received->value();
    ASSERT_TRUE(priorityAssignment.contains(result.task_id()))
        << "Received result for unexpected task_id: " << result.task_id();

    arrivalOrder.push_back(result.task_id());
  }

  // Every submitted task must come back exactly once; this is the part of
  // the test that must always hold, independent of scheduling/timing.
  std::vector<int32_t> expectedIds;
  for (const auto &spec : tasks) {
    expectedIds.push_back(spec.taskId);
  }
  std::vector<int32_t> sortedArrival = arrivalOrder;
  std::vector<int32_t> sortedExpected = expectedIds;
  std::sort(sortedArrival.begin(), sortedArrival.end());
  std::sort(sortedExpected.begin(), sortedExpected.end());
  EXPECT_EQ(sortedArrival, sortedExpected)
      << "Result set does not match the submitted task set.";

  // Whether arrival order actually reflects priority is timing-dependent
  // (it only shows up if more than one task was genuinely queued at once
  // inside the daemon's scheduler), so this is reported, not asserted.
  std::vector<int32_t> prioritySortedIds = expectedIds;
  std::sort(prioritySortedIds.begin(), prioritySortedIds.end(),
            [&](int32_t a, int32_t b) {
              return priorityAssignment[a] > priorityAssignment[b];
            });
  if (arrivalOrder == prioritySortedIds) {
    std::cout << "Arrival order matches priority order: scheduler reordered "
                 "these tasks.\n";
  } else {
    std::cout << "Arrival order does not match strict priority order "
                 "(expected if the daemon drained tasks faster than they "
                 "arrived, so few/none were ever queued together).\n";
  }
}

int main(int argc, char **argv) {
  std::vector<mqss::qrmci::test::DaemonProcess> daemons;
  daemons.emplace_back("qrmcid-standalone", QRMCI_STANDALONE_DAEMON_PATH,
                       QRMCI_DAEMON_LOG_DIR);
  return mqss::qrmci::test::runWithDaemons(argc, argv, std::move(daemons));
}
