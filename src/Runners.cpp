/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Runners.h"

#include "MQSSCIInterfaces/MQSSCompiler.h"
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

namespace {
mqss::mqssci::OptLevel getMQSSCIOptimizationLevel(int level) {
  switch (level) {
  case 1:
    return mqss::mqssci::OptLevel::O1;
  case 2:
    return mqss::mqssci::OptLevel::O2;
  case 3:
    return mqss::mqssci::OptLevel::O3;
  default:
    return mqss::mqssci::OptLevel::O3; // Default to O3 if invalid level
  }
}

std::expected<mqss::mqssci::ResultFormat, std::string>
getBackendCompatibleResultFormat(
    const mqss::qrmci::BackendWrapper &backendInfo) {
  /*
  mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED = 0;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2 = 1;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3 = 2;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QIR = 3;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING = 4;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE = 5;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING = 6;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE = 7;
  mqss::CircuitFormat::CIRCUIT_FORMAT_CALIBRATION = 8;
  mqss::CircuitFormat::CIRCUIT_FORMAT_QPY = 9;
  mqss::CircuitFormat::CIRCUIT_FORMAT_IQMJSON = 10;
  mqss::CircuitFormat::CIRCUIT_FORMAT_BATCHJOB = 11;
  */
  /*
  enum mqss::mqssci::ResultFormat { OPENQASM2, QIR, QIRBASE, QIRADAPTIVE,
  QIRFULL };
  */
  auto supportedFormats = backendInfo.getSupportedCircuitFormats();
  for (const auto &format : supportedFormats) {
    switch (format) {
    case mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2:
      return mqss::mqssci::ResultFormat::OPENQASM2;
    case mqss::CircuitFormat::CIRCUIT_FORMAT_QIR:
      return mqss::mqssci::ResultFormat::QIR;
    case mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING:
    case mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE:
      return mqss::mqssci::ResultFormat::QIRBASE;
    case mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING:
    case mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE:
      return mqss::mqssci::ResultFormat::QIRADAPTIVE;
    default:
      continue; // Skip unsupported formats
    }
  }
  return std::unexpected("No compatible result format found");
}

std::expected<void, std::string>
validateInputFormat(std::string_view circuitFileFormat,
                    std::vector<std::string_view> supportedInputFormats) {
  std::unordered_map<std::string_view, std::string_view> formatMapping = {
      {"quake", "cudaq-quake"},
      {"catalyst", "catalyst-quantum"},
  };
  auto it = formatMapping.find(circuitFileFormat);
  if (it != formatMapping.end()) {
    if (std::ranges::find(supportedInputFormats, it->second) !=
        supportedInputFormats.end()) {
      return {};
    }
  }
  return std::unexpected("Unsupported circuit file format: " +
                         std::string(circuitFileFormat));
}

} // namespace

std::expected<void, std::string> mqss::qrmci::selectBackend(
    mqss::QuantumTask &task,
    const std::unordered_map<std::string, mqss::qrmci::BackendWrapper>
        &availableBackends) {
  // This function should implement the logic to select a backend for the
  // quantum task. For now, we will just select the first available backend
  // that matches the preferred QPU.

  if (availableBackends.contains(task.preferred_qpu())) {
    task.set_scheduled_qpu(task.preferred_qpu());
    return {};
  }

  // If no preferred backend is found, set the scheduled QPU to an empty
  // string or handle it as needed.
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

  auto inputFormatValidation = validateInputFormat(
      task.circuit_file_type(),
      mqss::mqssci::MQSSCompiler::getSupportedInputFormats());
  if (!inputFormatValidation) {
    return std::unexpected(inputFormatValidation.error());
  }

  try {

    std::unordered_map<int, std::string> updatedCircuitFiles;
    auto inputCircuitFiles = task.circuit_files();
    mqss::mqssci::MQSSCompiler compiler;

    mqss::mqssci::CompilerOptions opts;
    opts.optimization_level =
        getMQSSCIOptimizationLevel(task.optimisation_level());
    auto resultFormat = getBackendCompatibleResultFormat(backendInfo);
    if (!resultFormat) {
      return std::unexpected(resultFormat.error());
    }
    opts.result_format = resultFormat.value();

    for (int i = 0; i < inputCircuitFiles.size(); ++i) {
      const auto &circuitFile = inputCircuitFiles[i];
      auto compiledCircuit =
          compiler.compileSource(circuitFile, "", backendInfo.getInstructions(),
                                 backendInfo.getQubitConnectivity(), opts);
      if (!compiledCircuit) {
        return std::unexpected("Compilation failed.");
      }
      updatedCircuitFiles[i] = compiledCircuit.value();
    }

    for (const auto &[index, newCircuitFile] : updatedCircuitFiles) {
      task.set_circuit_files(index, newCircuitFile);
    }
    return {}; // Compilation successful
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
