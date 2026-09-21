/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Runners.h"

#include "MQSSCIInterfaces/MQSSCompiler.h"
#include "TaskSubmission.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"
#include "qrmci/CircuitFormatPolicy.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <limits>
#include <mqss/submitter/Device.h>
#include <mqss/submitter/Job.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mqss::qrmci {

namespace {

/// @brief Whether @p candidate should displace @p incumbent under @p policy.
/// Both candidates are already known to be online and able to run the task.
/// Every policy ends in a name comparison, so the ordering is total and the
/// winner never depends on iteration order.
bool beatsIncumbent(const BackendWrapper &candidate,
                    const BackendWrapper &incumbent,
                    BackendSelectionPolicy policy) {
  switch (policy) {
  case BackendSelectionPolicy::SmallestSufficient:
    if (candidate.getNumQubits() != incumbent.getNumQubits()) {
      return candidate.getNumQubits() < incumbent.getNumQubits();
    }
    break;
  case BackendSelectionPolicy::LowestName:
    break;
  }
  return candidate.getName() < incumbent.getName();
}

/// @brief A flag that never becomes true, for the uninterruptible overloads
///        of submitQuantumTask()/collectQuantumResult()/executeQuantumTask()
///        to delegate into their interruptible counterparts with.
const std::atomic<bool> NeverTerminates{false};

/// @brief How long a single collectQuantumResult() wait slice waits before
///        re-checking the termination flag and the overall deadline.
constexpr std::chrono::seconds CollectionSlice{1};

} // namespace

std::expected<std::string, Error>
chooseBackend(const mqss::QuantumTask &task,
              const BackendRegistry &availableBackends,
              BackendSelectionPolicy policy) {

  // A usable preferred QPU takes precedence over the selection policy.
  if (!task.preferred_qpu().empty()) {
    if (const auto *preferred = availableBackends.find(task.preferred_qpu());
        preferred != nullptr && preferred->isOnline() &&
        preferred->canRun(task)) {
      return preferred->getName();
    }
  }

  const BackendWrapper *chosen = nullptr;
  availableBackends.forEachBackend([&](const std::string & /*backendName*/,
                                       const BackendWrapper &backendInfo) {
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

std::expected<void, Error>
compileQuantumTask(mqss::QuantumTask &task, const BackendWrapper &backendInfo,
                   mqss::mqssci::MQSSCompiler &compiler) {
  // A task marked no_modify is handed to the device exactly as it arrived.
  if (task.no_modify()) {
    return {};
  }

  const std::vector<std::string_view> supportedInputFormats =
      mqss::mqssci::MQSSCompiler::getSupportedInputFormats();
  if (auto validated = validateInputFormatIsSupportedCircuitFormat(
          task.circuit_file_type(), supportedInputFormats);
      !validated) {
    return std::unexpected(std::move(validated.error()));
  }

  auto target = backendInfo.compilerTarget();
  if (!target) {
    return std::unexpected(std::move(target.error()));
  }

  auto optimizationLevel =
      getCompilerOptimizationLevel(task.optimisation_level());
  if (!optimizationLevel) {
    return std::unexpected(std::move(optimizationLevel.error()));
  }

  const mqss::mqssci::CompilerOptions opts{
      .optimization_level = *optimizationLevel,
      .result_format = target->compilerFormat,
  };

  try {
    // Compiled into a staging vector rather than into the task itself: a
    // failure part-way through must leave the task's circuits as they were.
    std::vector<std::string> compiledCircuits;
    compiledCircuits.reserve(
        static_cast<std::size_t>(task.circuit_files().size()));

    for (const auto &circuitFile : task.circuit_files()) {
      const auto index = compiledCircuits.size();
      if (circuitFile.empty()) {
        // MQSSCompiler::compileSource() segfaults on an empty source instead
        // of returning a diagnostic (mqss-ci's CommonMappingPass dereferences
        // kernel-analysis state that a module with no kernels never
        // populates), so this must be rejected before it ever reaches the
        // compiler.
        return std::unexpected(Error{Error::Kind::CompilationFailed,
                                     "Compilation failed for circuit file " +
                                         std::to_string(index) +
                                         ": circuit is empty."});
      }

      auto compiled =
          compiler.compileSource(circuitFile, "", backendInfo.getInstructions(),
                                 backendInfo.getQubitConnectivity(), opts);
      if (!compiled) {
        // MQSSCompiler::compileSource() builds and owns its MLIRContext
        // entirely internally (see MQSSCIInterfaces/MQSSCompiler.cpp), so the
        // real MLIR diagnostic it emits on failure never reaches this caller
        // -- the circuit file index is the most specific detail available
        // here without changing that dependency's public API.
        return std::unexpected(Error{Error::Kind::CompilationFailed,
                                     "Compilation failed for circuit file " +
                                         std::to_string(index) + "."});
      }
      compiledCircuits.push_back(std::move(*compiled));
    }

    for (std::size_t index = 0; index < compiledCircuits.size(); ++index) {
      task.set_circuit_files(static_cast<int>(index), compiledCircuits[index]);
    }
    // The compiled bytes no longer match the task's original circuit type;
    // relabel it with the compiler target's own canonical label so
    // submission maps it back to the right circuit format.
    task.set_circuit_file_type(std::string(target->taskCircuitType));
    return {};
  } catch (const std::exception &e) {
    return std::unexpected(
        Error{Error::Kind::CompilationFailed,
              std::string("Compilation failed: ") + e.what()});
  }
}

std::expected<void, Error>
compileQuantumTask(mqss::QuantumTask &task, const BackendWrapper &backendInfo) {
  mqss::mqssci::MQSSCompiler compiler;
  return compileQuantumTask(task, backendInfo, compiler);
}

std::expected<mqss::QuantumResult, Error> buildQuantumResult(
    const mqss::QuantumTask &task,
    const std::vector<std::optional<mqss::submitter::Counts>> &histogram) {
  const auto expectedCircuits =
      static_cast<std::size_t>(task.circuit_files_size());
  if (histogram.size() != expectedCircuits) {
    return std::unexpected(Error{Error::Kind::DeviceError,
                                 "Execution failed: expected " +
                                     std::to_string(expectedCircuits) +
                                     " circuit result(s), got " +
                                     std::to_string(histogram.size()) + "."});
  }

  // Validated up front, before anything is written to the protobuf result: a
  // failure partway through must not leave a caller holding a partially
  // assembled QuantumResult.
  for (std::size_t index = 0; index < histogram.size(); ++index) {
    if (!histogram[index].has_value()) {
      return std::unexpected(
          Error{Error::Kind::DeviceError, "Execution failed: circuit " +
                                              std::to_string(index) +
                                              " produced no result."});
    }
    for (const auto &[key, value] : *histogram[index]) {
      if (value > static_cast<std::uint64_t>(
                      std::numeric_limits<std::int32_t>::max())) {
        return std::unexpected(
            Error{Error::Kind::DeviceError,
                  "Execution failed: count for '" + key + "' in circuit " +
                      std::to_string(index) + " is " + std::to_string(value) +
                      ", which exceeds the wire format's INT32_MAX limit."});
      }
    }
  }

  mqss::QuantumResult quantumResult;
  quantumResult.set_task_id(task.task_id());
  quantumResult.set_destination(task.result_destination());
  quantumResult.set_executed_qpu(task.scheduled_qpu());
  quantumResult.set_execution_status(true);
  quantumResult.set_additional_information("COMPLETED");
  quantumResult.mutable_results()->Reserve(static_cast<int>(histogram.size()));

  // One result entry per histogram entry, in order: cardinality validated
  // above means every entry has a value, so index alignment with the task's
  // circuit files is preserved rather than collapsed by skipping.
  for (const auto &counts : histogram) {
    auto *protoCounts = quantumResult.add_results()->mutable_counts();
    for (const auto &[key, value] : *counts) {
      (*protoCounts)[key] = static_cast<int32_t>(value);
    }
  }

  return quantumResult;
}

std::expected<std::vector<mqss::submitter::Job>, Error>
submitQuantumTask(const mqss::QuantumTask &task,
                  const mqss::submitter::Device &device) {
  return submitQuantumTask(task, device, NeverTerminates);
}

std::expected<std::vector<mqss::submitter::Job>, Error>
submitQuantumTask(const mqss::QuantumTask &task,
                  const mqss::submitter::Device &device,
                  const std::atomic<bool> &terminationFlag) {
  auto format = detail::validateSubmission(task);
  if (!format) {
    return std::unexpected(std::move(format.error()));
  }

  // A Job carries exactly one payload, so one job per circuit file, each run
  // for the task's own shot count.
  std::vector<mqss::submitter::Job> jobs;
  jobs.reserve(static_cast<std::size_t>(task.circuit_files().size()));
  for (int circuitIndex = 0; circuitIndex < task.circuit_files_size();
       ++circuitIndex) {
    if (terminationFlag.load(std::memory_order_relaxed)) {
      // Best-effort: a cancellation failure is logged, not propagated, since
      // shutdown must not stall waiting on a device that may already be gone.
      for (auto &submitted : jobs) {
        if (auto cancelled = submitted.cancel(); !cancelled) {
          spdlog::warn("Failed to cancel job for task {} during shutdown: {}",
                       task.task_id(), cancelled.error().message());
        }
      }
      return std::unexpected(
          Error{Error::Kind::ShutdownRequested,
                "Shutdown requested during task submission."});
    }

    auto job =
        device.submitJob(detail::makeJobRequest(task, circuitIndex, *format));
    if (!job) {
      // First failure wins: the jobs already submitted for this task are
      // dropped here, never waited on.
      return std::unexpected(
          toQrmciError(Error::Kind::SubmissionFailed, job.error()));
    }
    jobs.push_back(std::move(*job));
  }

  return jobs;
}

std::expected<mqss::QuantumResult, Error>
collectQuantumResult(const mqss::QuantumTask &task,
                     std::vector<mqss::submitter::Job> &jobs,
                     std::chrono::seconds waitTimeout) {
  return collectQuantumResult(task, jobs, waitTimeout, NeverTerminates);
}

std::expected<mqss::QuantumResult, Error>
collectQuantumResult(const mqss::QuantumTask &task,
                     std::vector<mqss::submitter::Job> &jobs,
                     std::chrono::seconds waitTimeout,
                     const std::atomic<bool> &terminationFlag) {
  const bool bounded = waitTimeout > std::chrono::seconds::zero();

  std::vector<std::optional<mqss::submitter::Counts>> histogram;
  histogram.reserve(jobs.size());

  for (auto &job : jobs) {
    const auto waitStarted = std::chrono::steady_clock::now();
    for (;;) {
      if (terminationFlag.load(std::memory_order_relaxed)) {
        return std::unexpected(
            Error{Error::Kind::ShutdownRequested,
                  "Shutdown requested while collecting a result."});
      }

      // Waited on in slices rather than for the whole timeout in one call, so
      // the termination flag above is re-checked between slices instead of
      // only before the first one.
      auto status = job.wait(CollectionSlice);
      if (status) {
        if (*status != mqss::submitter::JobStatus::Done) {
          // wait() returns Done or Canceled; cancellation produces no result.
          return std::unexpected(
              Error{Error::Kind::DeviceError,
                    "Job finished without producing a result (canceled)."});
        }
        break;
      }
      if (status.error().code() != mqss::submitter::ErrorCode::Timeout) {
        return std::unexpected(
            toQrmciError(Error::Kind::DeviceError, status.error()));
      }
      if (bounded &&
          std::chrono::steady_clock::now() - waitStarted >= waitTimeout) {
        return std::unexpected(
            Error{Error::Kind::DeviceError,
                  "Job did not finish within the configured wait timeout."});
      }
      // Otherwise: unbounded (waits indefinitely, one interruptible slice at
      // a time), or still within the deadline -- wait another slice.
    }

    auto counts = job.counts();
    if (!counts) {
      return std::unexpected(
          toQrmciError(Error::Kind::DeviceError, counts.error()));
    }
    histogram.emplace_back(std::move(*counts));
  }

  return buildQuantumResult(task, histogram);
}

std::expected<mqss::QuantumResult, Error>
executeQuantumTask(const mqss::QuantumTask &task,
                   const mqss::submitter::Device &device,
                   std::chrono::seconds waitTimeout) {
  return executeQuantumTask(task, device, waitTimeout, NeverTerminates);
}

std::expected<mqss::QuantumResult, Error>
executeQuantumTask(const mqss::QuantumTask &task,
                   const mqss::submitter::Device &device,
                   std::chrono::seconds waitTimeout,
                   const std::atomic<bool> &terminationFlag) {
  return submitQuantumTask(task, device, terminationFlag)
      .and_then([&](std::vector<mqss::submitter::Job> jobs) {
        return collectQuantumResult(task, jobs, waitTimeout, terminationFlag);
      });
}

mqss::QuantumResult cancelQuantumTask(const mqss::QuantumTask &task,
                                      const std::string &cancelReason) {
  mqss::QuantumResult quantumResult;
  quantumResult.set_task_id(task.task_id());
  quantumResult.set_destination(task.result_destination());
  quantumResult.set_executed_qpu(task.scheduled_qpu());
  quantumResult.set_execution_status(false);
  quantumResult.set_additional_information("CANCELLED: " + cancelReason);

  return quantumResult;
}

} // namespace mqss::qrmci
