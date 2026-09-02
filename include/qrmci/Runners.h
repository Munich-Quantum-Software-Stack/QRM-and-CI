/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "qdmi/constants.h"
#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <algorithm>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mqss::submitter {
class Submitter;
} // namespace mqss::submitter

namespace mqss::qrmci {

/// @file Runners.h
/// @brief Helper methods to run QRMCI components of objects.

/// @brief Choose the backend that should execute a quantum task.
///
/// A pure query: it neither reads nor writes the task's scheduled QPU, so
/// the caller assigns the returned name itself. The task's preferred QPU
/// wins whenever it is registered, online and able to run the task;
/// otherwise @p policy picks among every backend that can, deterministically.
/// @param task The quantum task to choose a backend for.
/// @param availableBackends The backends to choose from.
/// @param policy The rule for picking among several compatible backends.
/// @return The name of the chosen backend, or a NoBackendAvailable error if
/// no suitable backend is found.
[[nodiscard]] std::expected<std::string, Error>
chooseBackend(const mqss::QuantumTask &task,
              const mqss::qrmci::BackendRegistry &availableBackends,
              mqss::qrmci::BackendSelectionPolicy policy =
                  mqss::qrmci::BackendSelectionPolicy::LowestName);

/// @brief Compile the quantum task using @p compiler.
/// @param task The quantum task to compile.
/// @param backendInfo The backend information to use for compilation.
/// @param compiler The compiler to use for compilation.
/// @return Returns nothing on success, or an error describing why
/// compilation failed.
std::expected<void, Error>
compileQuantumTask(mqss::QuantumTask &task,
                   const mqss::qrmci::BackendWrapper &backendInfo,
                   mqss::mqssci::MQSSCompiler &compiler);

/// @brief Compile the quantum task using the real MQSS compiler
///        (mqss::mqssci::MQSSCompiler). A thin, out-of-line overload of the
///        three-argument compileQuantumTask() above -- defined in
///        Runners.cpp so every existing call site survives unchanged, with no
///        caller needing to construct a compiler itself.
/// @param task The quantum task to compile.
/// @param backendInfo The backend information to use for compilation.
/// @return Returns nothing on success, or an error describing why
/// compilation failed.
std::expected<void, Error>
compileQuantumTask(mqss::QuantumTask &task,
                   const mqss::qrmci::BackendWrapper &backendInfo);

/// @brief Build a QuantumResult from a task and its job-result histogram, the
///        shared tail end of submitQuantumTask()+collectQuantumResult() and
///        executeQuantumTask().
/// @param task The quantum task the histogram belongs to (used to populate
///        the result's task/destination/QPU fields).
/// @param histogram One counts map per circuit file, or nullopt for a
///        circuit that produced no counts.
/// @return An expected QuantumResult containing the execution results, or a
/// DeviceError if the histogram carries no valid results.
inline std::expected<mqss::QuantumResult, Error> buildQuantumResult(
    const mqss::QuantumTask &task,
    const std::vector<std::optional<std::map<std::string, size_t>>>
        &histogram) {
  mqss::QuantumResult quantumResult;
  quantumResult.set_task_id(task.task_id());
  quantumResult.set_destination(task.result_destination());
  quantumResult.set_executed_qpu(task.scheduled_qpu());

  if (std::ranges::any_of(
          histogram, [](const std::optional<std::map<std::string, size_t>> &r) {
            return r.has_value();
          })) {
    quantumResult.set_execution_status(true);
    quantumResult.set_additional_information("COMPLETED");
    quantumResult.mutable_results()->Reserve(
        static_cast<int>(histogram.size()));
    for (const auto &optMap : histogram) {
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
      Error{Error::Kind::DeviceError,
            "Execution failed: No valid results returned from the backend."});
}

/// @brief Submit the quantum task to the submitter without waiting for its
///        result. Pairs with collectQuantumResult() to let a caller submit
///        several tasks back-to-back before collecting any of their results
///        (pipelined batch submission), instead of blocking on each task's
///        result in turn the way executeQuantumTask() does.
/// @param task The quantum task to submit.
/// @param submitter The submitter to use for job submission.
/// @return The submitted job's ID, or a SubmissionFailed error if submission
/// fails.
std::expected<std::uint32_t, Error>
submitQuantumTask(const mqss::QuantumTask &task,
                  mqss::submitter::Submitter &submitter);

/// @brief Collect the result of a previously submitted quantum job. Pairs
///        with submitQuantumTask(); see that function's documentation for
///        why the two are split apart.
/// @param task The quantum task the job was submitted for (used to populate
///        the result's task/destination/QPU fields).
/// @param jobId The job ID returned by submitQuantumTask().
/// @param submitter The submitter to use for result retrieval.
/// @return An expected QuantumResult containing the execution results, an
/// Internal error if the job ID has no result to collect, or a DeviceError if
/// the device failed while producing it.
std::expected<mqss::QuantumResult, Error>
collectQuantumResult(const mqss::QuantumTask &task, std::uint32_t jobId,
                     mqss::submitter::Submitter &submitter);

/// @brief Execute the quantum task using the submitter: submits it and
///        collects its result in one call. Equivalent to
///        submitQuantumTask() followed by collectQuantumResult(); use those
///        directly to submit several tasks before collecting any of their
///        results (pipelined batch submission).
/// @param task The quantum task to execute.
/// @param submitter The submitter to use for task execution.
/// @return An expected QuantumResult containing the execution results, or the
/// error reported by whichever of the two steps failed.
std::expected<mqss::QuantumResult, Error>
executeQuantumTask(const mqss::QuantumTask &task,
                   mqss::submitter::Submitter &submitter);

/// @brief Cancel the quantum task and return a cancellation result.
/// @param task The quantum task to cancel.
/// @param cancelReason The reason for cancellation.
/// @return A QuantumResult indicating the task was cancelled, including the
/// cancellation reason.
mqss::QuantumResult cancelQuantumTask(const mqss::QuantumTask &task,
                                      const std::string &cancelReason);

} // namespace mqss::qrmci
