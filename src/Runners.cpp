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
#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

/// @brief Whether @p candidate should displace @p incumbent under @p policy.
/// Both candidates are already known to be online and able to run the task.
/// Every policy ends in a name comparison, so the ordering is total and the
/// winner never depends on iteration order.
bool beatsIncumbent(const mqss::qrmci::BackendWrapper &candidate,
                    const mqss::qrmci::BackendWrapper &incumbent,
                    mqss::qrmci::BackendSelectionPolicy policy) {
  switch (policy) {
  case mqss::qrmci::BackendSelectionPolicy::SmallestSufficient:
    if (candidate.getNumQubits() != incumbent.getNumQubits()) {
      return candidate.getNumQubits() < incumbent.getNumQubits();
    }
    break;
  case mqss::qrmci::BackendSelectionPolicy::LowestName:
    break;
  }
  return candidate.getName() < incumbent.getName();
}

} // namespace

std::expected<std::string, mqss::qrmci::Error> mqss::qrmci::chooseBackend(
    const mqss::QuantumTask &task,
    const mqss::qrmci::BackendRegistry &availableBackends,
    mqss::qrmci::BackendSelectionPolicy policy) {

  // The task's preferred QPU wins outright whenever it can actually run the
  // task, whatever the policy would otherwise have picked.
  if (const auto *preferred = availableBackends.find(task.preferred_qpu());
      preferred != nullptr && preferred->isOnline() &&
      preferred->canRun(task)) {
    return preferred->getName();
  }

  const mqss::qrmci::BackendWrapper *chosen = nullptr;
  availableBackends.forEachBackend(
      [&](const std::string & /*backendName*/,
          const mqss::qrmci::BackendWrapper &backendInfo) {
        if (!backendInfo.isOnline() || !backendInfo.canRun(task)) {
          return;
        }
        if (chosen == nullptr || beatsIncumbent(backendInfo, *chosen, policy)) {
          chosen = &backendInfo;
        }
      });

  if (chosen == nullptr) {
    return std::unexpected(
        Error{Error::Kind::NoBackendAvailable, "No suitable backend found"});
  }
  return chosen->getName();
}

std::expected<void, mqss::qrmci::Error>
mqss::qrmci::compileQuantumTask(mqss::QuantumTask &task,
                                const mqss::qrmci::BackendWrapper &backendInfo,
                                mqss::mqssci::MQSSCompiler &compiler) {
  if (task.no_modify()) {
    // If the task is marked as no_modify, we skip compilation and return
    // success.
    return {};
  }

  const std::vector<std::string_view> supportedInputFormats =
      mqss::mqssci::MQSSCompiler::getSupportedInputFormats();
  auto inputFormatValidation = validateInputFormatIsSupportedCircuitFormat(
      task.circuit_file_type(), supportedInputFormats);
  if (!inputFormatValidation) {
    return std::unexpected(inputFormatValidation.error());
  }

  try {
    std::unordered_map<int, std::string> updatedCircuitFiles;
    auto inputCircuitFiles = task.circuit_files();

    mqss::mqssci::CompilerOptions opts;
    opts.optimization_level =
        getCompilerOptimizationLevel(task.optimisation_level());
    auto resultFormat = backendInfo.compilerResultFormat();
    if (!resultFormat) {
      return std::unexpected(resultFormat.error());
    }
    opts.result_format = resultFormat.value();

    for (int i = 0; i < static_cast<int>(inputCircuitFiles.size()); ++i) {
      const auto &circuitFile = inputCircuitFiles[i];
      if (circuitFile.empty()) {
        // MQSSCompiler::compileSource() segfaults on an empty source instead
        // of returning a diagnostic (mqss-ci's CommonMappingPass dereferences
        // kernel-analysis state that a module with no kernels never
        // populates), so this must be rejected before it ever reaches the
        // compiler.
        return std::unexpected(Error{Error::Kind::CompilationFailed,
                                     "Compilation failed for circuit file " +
                                         std::to_string(i) +
                                         ": circuit is empty."});
      }
      auto compiledCircuit =
          compiler.compileSource(circuitFile, "", backendInfo.getInstructions(),
                                 backendInfo.getQubitConnectivity(), opts);
      if (!compiledCircuit) {
        // MQSSCompiler::compileSource() builds and owns its MLIRContext
        // entirely internally (see MQSSCIInterfaces/MQSSCompiler.cpp), so the
        // real MLIR diagnostic it emits on failure never reaches this caller
        // -- the circuit file index is the most specific detail available
        // here without changing that dependency's public API.
        return std::unexpected(Error{Error::Kind::CompilationFailed,
                                     "Compilation failed for circuit file " +
                                         std::to_string(i) + "."});
      }
      updatedCircuitFiles[i] = compiledCircuit.value();
    }

    for (const auto &[index, newCircuitFile] : updatedCircuitFiles) {
      task.set_circuit_files(index, newCircuitFile);
    }
    return {}; // Compilation successful
  } catch (const std::exception &e) {
    return std::unexpected(
        Error{Error::Kind::CompilationFailed,
              std::string("Compilation failed: ") + e.what()});
  }
}

std::expected<void, mqss::qrmci::Error> mqss::qrmci::compileQuantumTask(
    mqss::QuantumTask &task, const mqss::qrmci::BackendWrapper &backendInfo) {
  mqss::mqssci::MQSSCompiler compiler;
  return mqss::qrmci::compileQuantumTask(task, backendInfo, compiler);
}

std::expected<std::uint32_t, mqss::qrmci::Error>
mqss::qrmci::submitQuantumTask(const mqss::QuantumTask &task,
                               mqss::submitter::Submitter &submitter) {
  try {
    return submitter.submitJob(
        {task.circuit_files().begin(), task.circuit_files().end()},
        static_cast<size_t>(task.n_shots()), QDMI_PROGRAM_FORMAT_QIRBASESTRING);
  } catch (const std::exception &e) {
    return std::unexpected(
        Error{Error::Kind::SubmissionFailed,
              std::string("Submission failed: ") + e.what()});
  }
}

std::expected<mqss::QuantumResult, mqss::qrmci::Error>
mqss::qrmci::collectQuantumResult(const mqss::QuantumTask &task,
                                  std::uint32_t jobId,
                                  mqss::submitter::Submitter &submitter) {
  try {
    auto histogram = submitter.getJobResultHistogram(jobId);
    if (!histogram) {
      // Submitter::getJobResultHistogram() erases a job from activeJobs once
      // retrieved, so nullopt here means this job ID was already consumed or
      // never existed -- kept explicit rather than a bare .value() since a
      // caller's usage pattern isn't guaranteed to be strictly sequential.
      return std::unexpected(Error{
          Error::Kind::Internal,
          "Execution failed: no result found for job " + std::to_string(jobId) +
              " (it may already have been retrieved or was "
              "never submitted)."});
    }

    return buildQuantumResult(task, *histogram);
  } catch (const std::exception &e) {
    return std::unexpected(Error{Error::Kind::DeviceError,
                                 std::string("Execution failed: ") + e.what()});
  }
}

std::expected<mqss::QuantumResult, mqss::qrmci::Error>
mqss::qrmci::executeQuantumTask(const mqss::QuantumTask &task,
                                mqss::submitter::Submitter &submitter) {
  return submitQuantumTask(task, submitter).and_then([&](std::uint32_t jobId) {
    return collectQuantumResult(task, jobId, submitter);
  });
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
