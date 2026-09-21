/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file TaskSubmission.h
/// @brief Private task-submission validation and request construction shared
///        by the interruptible `submitQuantumTask()` runner and
///        `TaskExecution`. Kept separate from `Runners.h`/`TaskExecution.h`:
///        this is the one place a task's shape is checked and turned into
///        wire requests, not a public entry point of its own.

#pragma once

#include "mqss/Protocol.hpp"
#include "qrmci/Error.h"

#include <expected>
#include <mqss/submitter/Job.h>

namespace mqss::qrmci::detail {

/// @brief Check that @p task has circuit files, a positive shot count, and a
///        circuit file type with a known direct submission format.
/// @param task The task to validate.
/// @return The task's circuit format, or a SubmissionFailed error naming
///         which of those checks failed.
[[nodiscard]] std::expected<mqss::CircuitFormat, Error>
validateSubmission(const mqss::QuantumTask &task);

/// @brief Build the wire request for one of a validated task's circuit
///        files.
/// @param task A task that has already passed validateSubmission().
/// @param circuitIndex A valid index into @p task's circuit files.
/// @param format The format validateSubmission() returned for @p task.
/// @return The JobRequest carrying that circuit's opaque payload bytes and
///         the task's shot count.
[[nodiscard]] mqss::submitter::JobRequest
makeJobRequest(const mqss::QuantumTask &task, int circuitIndex,
               mqss::CircuitFormat format);

} // namespace mqss::qrmci::detail
