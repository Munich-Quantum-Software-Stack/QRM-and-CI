/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// Submits several tasks back to back, before waiting on any of their
// results, to opportunistically exercise the distributed worker's pipelined
// batch execution (apps/distributed/worker/main.cpp submits every ready job
// via submitQuantumTask() before collecting any result via
// collectQuantumResult(), so several jobs can be in flight on the device at
// once). Whether or not the worker actually batches this particular run is a
// timing detail; what this test asserts unconditionally is the invariant
// that matters to a caller regardless of batching: concurrently in-flight
// tasks must not have their results mixed up. Each task gets its own
// result_destination queue so a mismatch is caught directly rather than
// inferred from delivery order. Works against apps/standalone too, since it
// shares the same front-door queue, but the pipelining this is meant to
// exercise is specific to apps/distributed's worker -- this binary still
// spawns apps/standalone's daemon (like every other single-daemon test
// here) since only the invariant, not the batching itself, is asserted.
//
// Task kinds are interleaved (success / unsupported backend / compile
// failure, repeated) rather than grouped, so a mix-up would have to land on
// two tasks with different expected outcomes to go unnoticed -- a stronger
// signal than an all-success or all-failure batch. See
// TestSubmitTaskUnsupportedBackend.cpp and TestSubmitTaskCompileFailure.cpp
// for why these two particular inputs deterministically force each
// cancellation path.

#include "DaemonProcess.h"
#include "IntegrationTestHelpers.h"
#include "RunWithDaemons.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

enum class TaskKind {
  Success,
  UnsupportedBackend,
  CompileFailure,
};

struct ExpectedTask {
  mqss::QuantumTask task;
  TaskKind kind;
};

constexpr int kKindRepeats = 2;

std::string kindLabel(TaskKind kind) {
  switch (kind) {
  case TaskKind::Success:
    return "success";
  case TaskKind::UnsupportedBackend:
    return "unsupported_backend";
  case TaskKind::CompileFailure:
    return "compile_failure";
  }
  return "unknown";
}

// Builds a task whose parameters deterministically produce the given kind's
// outcome, so the response can be checked against what was expected instead
// of just against what was sent.
mqss::QuantumTask makeTask(TaskKind kind, std::int32_t taskId) {
  mqss::QuantumTask qtask;
  qtask.set_task_id(taskId);
  qtask.set_n_shots(100);
  qtask.set_optimisation_level(1);
  qtask.set_preferred_qpu(std::string("C++ Device with 5 qubits"));
  qtask.set_no_modify(false);
  qtask.set_result_destination("tester.tasks.queue.concurrent." +
                               kindLabel(kind) + "." + std::to_string(taskId));

  switch (kind) {
  case TaskKind::Success:
    qtask.add_circuit_files(mqss::qrmci::test::sampleQuakeCircuit());
    qtask.set_circuit_file_type(std::string("quake"));
    break;
  case TaskKind::UnsupportedBackend:
    qtask.add_circuit_files(
        std::string("irrelevant: no backend supports this format"));
    qtask.set_circuit_file_type(std::string("unsupported-format-xyz"));
    break;
  case TaskKind::CompileFailure:
    qtask.add_circuit_files(std::string(""));
    qtask.set_circuit_file_type(std::string("quake"));
    break;
  }
  return qtask;
}

class SubmitTaskConcurrentTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto loadedConfig = mqss::qrmci::loadConfig();
    ASSERT_TRUE(loadedConfig) << loadedConfig.error().detail;
    config = *loadedConfig;
  }

  mqss::qrmci::Config config;
};

TEST_F(SubmitTaskConcurrentTest, KeepsConcurrentResultsSeparate) {
  constexpr TaskKind kKindOrder[] = {
      TaskKind::Success,
      TaskKind::UnsupportedBackend,
      TaskKind::CompileFailure,
  };

  std::vector<ExpectedTask> tasks;
  tasks.reserve(kKindRepeats * std::size(kKindOrder));
  for (int repeat = 0; repeat < kKindRepeats; ++repeat) {
    for (const TaskKind kind : kKindOrder) {
      tasks.push_back(ExpectedTask{
          makeTask(kind, mqss::qrmci::test::uniqueTaskId()), kind});
    }
  }

  mqss::qrmci::CommunicationHandler communicationHandler(
      config.common.connection);

  for (const auto &expected : tasks) {
    auto sent =
        communicationHandler.send(expected.task, config.common.qrmciQueue);
    ASSERT_TRUE(sent) << "Sending " << kindLabel(expected.kind)
                      << " task_id=" << expected.task.task_id() << ": "
                      << sent.error().detail;
  }

  for (const auto &expected : tasks) {
    const auto &qtask = expected.task;
    SCOPED_TRACE("task_id=" + std::to_string(qtask.task_id()) +
                 " kind=" + kindLabel(expected.kind));

    auto received = communicationHandler.receive<mqss::QuantumResult>(
        qtask.result_destination(), mqss::qrmci::test::kPolledResultTimeout);
    if (!received.has_value()) {
      ADD_FAILURE() << "Could not read from queue "
                    << qtask.result_destination() << ": "
                    << received.error().detail;
      continue;
    }
    if (!received->has_value()) {
      ADD_FAILURE() << "No result received on queue "
                    << qtask.result_destination();
      continue;
    }

    const mqss::QuantumResult &taskResult = received->value();
    EXPECT_EQ(taskResult.task_id(), qtask.task_id())
        << "Result on queue " << qtask.result_destination()
        << " belongs to a different task_id than expected.";

    switch (expected.kind) {
    case TaskKind::Success:
      EXPECT_TRUE(taskResult.execution_status())
          << "Expected to execute successfully but was cancelled: "
          << taskResult.additional_information();
      break;
    case TaskKind::UnsupportedBackend: {
      const std::string expectedPrefix = "CANCELLED: No suitable backend found";
      EXPECT_FALSE(taskResult.execution_status());
      EXPECT_TRUE(
          taskResult.additional_information().starts_with(expectedPrefix))
          << "Expected cancellation starting with '" << expectedPrefix
          << "', got: '" << taskResult.additional_information() << "'";
      break;
    }
    case TaskKind::CompileFailure: {
      const std::string expectedPrefix =
          "CANCELLED: Compilation failed for circuit file 0: circuit is "
          "empty.";
      EXPECT_FALSE(taskResult.execution_status());
      EXPECT_TRUE(
          taskResult.additional_information().starts_with(expectedPrefix))
          << "Expected cancellation starting with '" << expectedPrefix
          << "', got: '" << taskResult.additional_information() << "'";
      break;
    }
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  std::vector<mqss::qrmci::test::DaemonProcess> daemons;
  daemons.emplace_back("qrmcid-standalone", QRMCI_STANDALONE_DAEMON_PATH,
                       QRMCI_DAEMON_LOG_DIR);
  return mqss::qrmci::test::runWithDaemons(argc, argv, std::move(daemons));
}
