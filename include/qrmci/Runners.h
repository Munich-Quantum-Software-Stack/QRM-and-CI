/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "BackendWrapper.h"
#include "Submitter.h"

#include <expected>
#include <string>
#include <unordered_map>

namespace mqss::qrmci {

/// @file Runners.h
/// @brief Helper methods to run QRMCI components of objects.

/// @brief Selects the backend that should execute a quantum task.
/// @param task Update the quantum task with the selected backend.
/// @param availableBackends A map of available backends, keyed by backend name.
/// @return Returns nothing on success, or an error message if no suitable
/// backend is found.
/// @note This function modifies the task in place to set the selected backend.
std::expected<void, std::string>
selectBackend(mqss::QuantumTask &task,
              const std::unordered_map<std::string, mqss::qrmci::BackendWrapper>
                  &availableBackends);

/// @brief Compile the quantum task using the MQSS compiler.
/// @param task The quantum task to compile.
/// @param backendInfo The backend information to use for compilation.
/// @return Returns nothing on success, or an error message if compilation
/// fails.
std::expected<void, std::string>
compileQuantumTask(mqss::QuantumTask &task,
                   const mqss::qrmci::BackendWrapper &backendInfo);

/// @brief Execute the quantum task using the submitter.
/// @param task The quantum task to execute.
/// @param submitter The submitter to use for task execution.
/// @return An expected QuantumResult containing the execution results or an
/// error message if the execution fails.
std::expected<mqss::QuantumResult, std::string>
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
