/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Runners.h"

#include "MQSSCompiler.h"
#include "Submitter.h"
#include "mqss/Protocol.hpp"
#include "qdmi/constants.h"
#include "qrmci/BackendWrapper.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

std::expected<void, std::string> mqss::qrmci::selectBackend(
    mqss::QuantumTask &task,
    const std::unordered_map<std::string, mqss::qrmci::BackendWrapper>
        &availableBackends) {
  // This function should implement the logic to select a backend for the
  // quantum task. For now, we will just select the first available backend that
  // matches the preferred QPU.

  if (availableBackends.contains(task.preferred_qpu())) {
    task.set_scheduled_qpu(task.preferred_qpu());
    return {};
  }

  // If no preferred backend is found, set the scheduled QPU to an empty string
  // or handle it as needed.
  task.set_scheduled_qpu("");
  return std::unexpected("No suitable backend found");
}

std::expected<void, std::string> mqss::qrmci::compileQuantumTask(
    mqss::QuantumTask &task, const mqss::qrmci::BackendWrapper &backendInfo) {

  if (task.no_modify()) {
    // If the task is marked as no_modify, we skip compilation and return
    // success.
    return {};
  }

  try {

    std::unordered_map<int, std::string> updatedCircuitFiles;
    auto inputCircuitFiles = task.circuit_files();
    MQSSCompiler compiler;

    for (int i = 0; i < inputCircuitFiles.size(); ++i) {
      CompilerOptions opts;
      opts.optimizationLevel = task.optimisation_level();
      // result_type can be set to:
      // "qir", "qir-full", "qir-adaptive", "qir-base", "openqasm2"
      opts.resultType = "openqasm2";
      const auto &circuitFile = inputCircuitFiles[i];

      updatedCircuitFiles[i] =
          compiler.compile(circuitFile, backendInfo.getInstructions(),
                           backendInfo.getQubitConnectivity(), opts);
    }

    for (const auto &[index, newCircuitFile] : updatedCircuitFiles) {
      task.set_circuit_files(index, newCircuitFile);
    }
    return {};
  } catch (const std::exception &e) {
    return std::unexpected(std::string("Compilation failed: ") + e.what());
  }
}

std::expected<mqss::QuantumResult, std::string>
mqss::qrmci::executeQuantumTask(const mqss::QuantumTask &task,
                                mqss::submitter::Submitter &submitter) {
  try {
    auto results =
        submitter
            .getJobResultHistogram(submitter.submitJob(
                {task.circuit_files().begin(), task.circuit_files().end()},
                static_cast<size_t>(task.n_shots()),
                QDMI_PROGRAM_FORMAT_QIRBASESTRING))
            .value();

    mqss::QuantumResult quantumResult;
    quantumResult.set_task_id(task.task_id());
    quantumResult.set_destination(task.result_destination());
    quantumResult.set_executed_qpu(task.scheduled_qpu());

    if (std::ranges::any_of(
            results, [](const std::optional<std::map<std::string, size_t>> &r) {
              return r.has_value();
            })) {
      quantumResult.set_execution_status(true);
      quantumResult.set_additional_information("COMPLETED");
      quantumResult.mutable_results()->Reserve(
          static_cast<int>(results.size()));
      for (const auto &optMap : results) {
        if (optMap.has_value()) {
          auto *protoCounts = quantumResult.add_results()->mutable_counts();
          for (const auto &[key, val] : *optMap) {
            (*protoCounts)[key] = static_cast<int32_t>(val);
          }
        }
      }

      return quantumResult;
    }

    return std::unexpected(
        "Execution failed: No valid results returned from the backend.");

  } catch (const std::exception &e) {
    return std::unexpected(std::string("Execution failed: ") + e.what());
  }
}

mqss::QuantumResult
mqss::qrmci::cancelQuantumTask(const mqss::QuantumTask &task,
                               const std::string &cancelReason) {
  mqss::QuantumResult quantumResult;
  quantumResult.set_task_id(task.task_id());
  quantumResult.set_destination(task.result_destination());
  quantumResult.set_executed_qpu(task.scheduled_qpu());
  quantumResult.set_execution_status(false);
  quantumResult.set_additional_information("CANCELLED: " + cancelReason);

  return quantumResult;
}
