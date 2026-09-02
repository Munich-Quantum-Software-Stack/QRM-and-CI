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
// exercise is specific to apps/distributed's worker.
//
// Task kinds are interleaved (success / unsupported backend / compile
// failure, repeated) rather than grouped, so a mix-up would have to land on
// two tasks with different expected outcomes to go unnoticed -- a stronger
// signal than an all-success or all-failure batch. See
// submit_task_unsupported_backend.cpp and submit_task_compile_failure.cpp
// for why these two particular inputs deterministically force each
// cancellation path.

#include "IntegrationTestHelpers.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <chrono>
#include <iostream>
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

constexpr std::int32_t kFirstTaskId = 2001;
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

} // namespace

int main() {

  auto config = mqss::qrmci::loadConfig();
  if (!config) {
    std::cerr << config.error().detail << "\n";
    return 1;
  }

  constexpr TaskKind kKindOrder[] = {
      TaskKind::Success,
      TaskKind::UnsupportedBackend,
      TaskKind::CompileFailure,
  };

  std::vector<ExpectedTask> tasks;
  tasks.reserve(kKindRepeats * std::size(kKindOrder));
  std::int32_t nextTaskId = kFirstTaskId;
  for (int repeat = 0; repeat < kKindRepeats; ++repeat) {
    for (const TaskKind kind : kKindOrder) {
      tasks.push_back(ExpectedTask{makeTask(kind, nextTaskId), kind});
      ++nextTaskId;
    }
  }

  mqss::qrmci::CommunicationHandler communicationHandler(
      config->common.connection);

  for (const auto &expected : tasks) {
    std::cout << "Sending " << kindLabel(expected.kind)
              << " task with task_id: " << expected.task.task_id()
              << " to queue: " << config->common.qrmciQueue << "\n";
    if (auto sent =
            communicationHandler.send(expected.task, config->common.qrmciQueue);
        !sent) {
      std::cerr << sent.error().detail << "\n";
      return 1;
    }
  }

  int failureCount = 0;
  for (const auto &expected : tasks) {
    const auto &qtask = expected.task;
    std::cout << "Waiting for result from queue: " << qtask.result_destination()
              << "\n";
    auto received = communicationHandler.receive<mqss::QuantumResult>(
        qtask.result_destination(), std::chrono::milliseconds(0));
    if (!received.has_value()) {
      std::cerr << "Could not read from queue " << qtask.result_destination()
                << ": " << received.error().detail << "\n";
      ++failureCount;
      continue;
    }
    const auto &optResult = *received;

    if (!optResult.has_value()) {
      std::cerr << "No result received for task_id " << qtask.task_id()
                << " on queue " << qtask.result_destination() << "\n";
      ++failureCount;
      continue;
    }

    const mqss::QuantumResult &taskResult = optResult.value();
    if (taskResult.task_id() != qtask.task_id()) {
      std::cerr << "Result on queue " << qtask.result_destination()
                << " belongs to task_id " << taskResult.task_id()
                << ", expected " << qtask.task_id() << "\n";
      ++failureCount;
      continue;
    }

    switch (expected.kind) {
    case TaskKind::Success: {
      if (!taskResult.execution_status()) {
        std::cerr << "Task " << taskResult.task_id()
                  << " expected to execute successfully but was cancelled: "
                  << taskResult.additional_information() << "\n";
        ++failureCount;
        continue;
      }
      break;
    }
    case TaskKind::UnsupportedBackend: {
      const std::string expectedPrefix = "CANCELLED: No suitable backend found";
      if (taskResult.execution_status() ||
          !taskResult.additional_information().starts_with(expectedPrefix)) {
        std::cerr << "Task " << taskResult.task_id()
                  << " expected cancellation starting with '" << expectedPrefix
                  << "', got execution_status=" << taskResult.execution_status()
                  << ", additional_information='"
                  << taskResult.additional_information() << "'\n";
        ++failureCount;
        continue;
      }
      break;
    }
    case TaskKind::CompileFailure: {
      const std::string expectedPrefix =
          "CANCELLED: Compilation failed for circuit file 0: circuit is "
          "empty.";
      if (taskResult.execution_status() ||
          !taskResult.additional_information().starts_with(expectedPrefix)) {
        std::cerr << "Task " << taskResult.task_id()
                  << " expected cancellation starting with '" << expectedPrefix
                  << "', got execution_status=" << taskResult.execution_status()
                  << ", additional_information='"
                  << taskResult.additional_information() << "'\n";
        ++failureCount;
        continue;
      }
      break;
    }
    }

    std::cout << "Received expected " << kindLabel(expected.kind)
              << " result for task_id: " << taskResult.task_id() << "\n";
  }

  if (failureCount > 0) {
    std::cerr << failureCount << " of " << tasks.size()
              << " concurrent tasks did not match their expected outcome."
              << "\n";
    return 1;
  }
}
