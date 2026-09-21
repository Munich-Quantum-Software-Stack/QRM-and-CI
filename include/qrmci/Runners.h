/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file Runners.h
/// @brief The QRM&CI pipeline stages as free functions: choosing a backend,
///        compiling a task for it, submitting it to a device, collecting its
///        result, and reporting a cancellation. An entrypoint composes these
///        into whatever loop it needs; none of them owns state of its own.

#pragma once

#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <atomic>
#include <chrono>
#include <expected>
#include <mqss/submitter/Job.h>
#include <optional>
#include <string>
#include <vector>

namespace mqss::submitter {
class Device;
} // namespace mqss::submitter

namespace mqss::qrmci {

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
[[nodiscard]] std::expected<std::string, Error> chooseBackend(
    const mqss::QuantumTask &task, const BackendRegistry &availableBackends,
    BackendSelectionPolicy policy = BackendSelectionPolicy::LowestName);

/// @brief Compile the quantum task using @p compiler.
/// @param task The quantum task to compile.
/// @param backendInfo The backend information to use for compilation.
/// @param compiler The compiler to use for compilation.
/// @return Returns nothing on success, or an error describing why
/// compilation failed.
[[nodiscard]] std::expected<void, Error>
compileQuantumTask(mqss::QuantumTask &task, const BackendWrapper &backendInfo,
                   mqss::mqssci::MQSSCompiler &compiler);

/// @brief Compile the quantum task with a compiler of this function's own,
///        for callers that have no reason to own one.
///
/// A caller that compiles repeatedly should construct an
/// mqss::mqssci::MQSSCompiler once and use the three-argument overload
/// instead, rather than paying for a fresh compiler per task.
/// @param task The quantum task to compile.
/// @param backendInfo The backend information to use for compilation.
/// @return Returns nothing on success, or an error describing why
/// compilation failed.
[[nodiscard]] std::expected<void, Error>
compileQuantumTask(mqss::QuantumTask &task, const BackendWrapper &backendInfo);

/// @brief Build a QuantumResult from a task and its job-result histogram, the
///        shared tail end of submitQuantumTask()+collectQuantumResult() and
///        executeQuantumTask().
///
/// @p histogram's length must equal @p task's circuit file count, and every
/// entry must carry counts: cardinality is preserved rather than collapsed,
/// so the result has exactly one entry per circuit, in the same order, on
/// success. Every count must also fit in the wire format's `int32_t`.
/// Validated up front, before anything is written to the result, so a
/// failure never returns a partially assembled QuantumResult.
/// @param task The quantum task the histogram belongs to (used to populate
///        the result's task/destination/QPU fields).
/// @param histogram One counts map per circuit file, or nullopt for a
///        circuit that produced no counts.
/// @return An expected QuantumResult containing one result entry per circuit,
/// or a DeviceError if the histogram's length does not match the task's
/// circuit count, any entry is nullopt, or any count exceeds `INT32_MAX` --
/// each case naming the offending circuit index, or bitstring and value.
[[nodiscard]] std::expected<mqss::QuantumResult, Error> buildQuantumResult(
    const mqss::QuantumTask &task,
    const std::vector<std::optional<mqss::submitter::Counts>> &histogram);

/// @brief Submit the quantum task's circuits to the device without waiting for
///        their results. Pairs with collectQuantumResult() to let a caller
///        submit several tasks back-to-back before collecting any of their
///        results (pipelined batch submission), instead of blocking on each
///        task's result in turn the way executeQuantumTask() does.
///
/// A Job carries exactly one payload, so a task with N circuit files becomes N
/// jobs, submitted in the files' own order and returned in that order. The
/// first submission failure short-circuits without waiting on or collecting
/// any job already submitted for this task.
///
/// Uninterruptible: unsuitable for a daemon work loop that must shut down
/// promptly. Delegates to the three-argument overload with a flag that is
/// never set.
/// @param task The quantum task to submit.
/// @param device The device to submit to.
/// @return The submitted jobs, one per circuit file, or a SubmissionFailed
/// error -- which also covers a task with no circuit files or a zero shot
/// count, both refused before the device is touched.
[[nodiscard]] std::expected<std::vector<mqss::submitter::Job>, Error>
submitQuantumTask(const mqss::QuantumTask &task,
                  const mqss::submitter::Device &device);

/// @brief Interruptible overload of submitQuantumTask(), for a daemon work
///        loop composing its own termination flag.
///
/// @p terminationFlag is checked before each circuit's submission. If it is
/// set, every job already submitted for this task is best-effort cancelled
/// (a cancellation failure is logged, not propagated) and the call returns
/// `Error::Kind::ShutdownRequested` instead of submitting the rest.
/// @param task The quantum task to submit.
/// @param device The device to submit to.
/// @param terminationFlag Consulted before every circuit submission.
/// @return The submitted jobs, one per circuit file; a SubmissionFailed error
/// as the two-argument overload describes; or `ShutdownRequested` if
/// @p terminationFlag was set before every circuit could be submitted.
[[nodiscard]] std::expected<std::vector<mqss::submitter::Job>, Error>
submitQuantumTask(const mqss::QuantumTask &task,
                  const mqss::submitter::Device &device,
                  const std::atomic<bool> &terminationFlag);

/// @brief Collect the results of previously submitted quantum jobs. Pairs
///        with submitQuantumTask(); see that function's documentation for
///        why the two are split apart.
///
/// Uninterruptible: unsuitable for a daemon work loop that must shut down
/// promptly. Delegates to the four-argument overload with a flag that is
/// never set.
/// @param task The quantum task the jobs were submitted for (used to populate
///        the result's task/destination/QPU fields).
/// @param jobs The jobs returned by submitQuantumTask(), waited on in order.
/// @param waitTimeout How long to wait for each job; zero waits indefinitely.
/// @return An expected QuantumResult containing one result entry per job, or
/// a DeviceError if a wait or a counts read failed, if a job finished in any
/// state other than Done, or if a count exceeds `INT32_MAX`
/// (buildQuantumResult()'s own checks; @p jobs' length already matches the
/// task's circuit count here, so cardinality itself cannot mismatch).
[[nodiscard]] std::expected<mqss::QuantumResult, Error>
collectQuantumResult(const mqss::QuantumTask &task,
                     std::vector<mqss::submitter::Job> &jobs,
                     std::chrono::seconds waitTimeout);

/// @brief Interruptible overload of collectQuantumResult(), for a daemon work
///        loop composing its own termination flag.
///
/// Each job is waited on in slices of at most one second rather than in one
/// blocking call, so @p terminationFlag is checked, and a shutdown honoured,
/// between slices instead of only before the whole wait. A zero @p
/// waitTimeout still waits indefinitely absent a shutdown, but is now
/// interruptible rather than truly unbounded; a positive @p waitTimeout still
/// times out at the same total deadline as the three-argument overload.
/// @param task The quantum task the jobs were submitted for.
/// @param jobs The jobs returned by submitQuantumTask(), waited on in order.
/// @param waitTimeout How long to wait for each job; zero waits indefinitely.
/// @param terminationFlag Consulted before every wait slice.
/// @return An expected QuantumResult as the three-argument overload
/// describes, or `ShutdownRequested` if @p terminationFlag was set before
/// every job finished.
[[nodiscard]] std::expected<mqss::QuantumResult, Error> collectQuantumResult(
    const mqss::QuantumTask &task, std::vector<mqss::submitter::Job> &jobs,
    std::chrono::seconds waitTimeout, const std::atomic<bool> &terminationFlag);

/// @brief Execute the quantum task on the device: submits it and collects its
///        result in one call. Equivalent to submitQuantumTask() followed by
///        collectQuantumResult(); use those directly to submit several tasks
///        before collecting any of their results (pipelined batch
///        submission).
///
/// Uninterruptible: unsuitable for a daemon work loop that must shut down
/// promptly. Delegates to the four-argument overload with a flag that is
/// never set.
/// @param task The quantum task to execute.
/// @param device The device to execute on.
/// @param waitTimeout How long to wait for each job; zero waits indefinitely.
/// @return An expected QuantumResult containing the execution results, or the
/// error reported by whichever of the two steps failed.
[[nodiscard]] std::expected<mqss::QuantumResult, Error>
executeQuantumTask(const mqss::QuantumTask &task,
                   const mqss::submitter::Device &device,
                   std::chrono::seconds waitTimeout);

/// @brief Interruptible overload of executeQuantumTask(), composing the
///        interruptible submitQuantumTask()/collectQuantumResult() overloads.
/// @param task The quantum task to execute.
/// @param device The device to execute on.
/// @param waitTimeout How long to wait for each job; zero waits indefinitely.
/// @param terminationFlag Consulted by both the submission and collection
///        stages; see their own documentation for exactly when.
/// @return An expected QuantumResult as the three-argument overload
/// describes, or `ShutdownRequested` if @p terminationFlag was set before
/// submission or collection finished.
[[nodiscard]] std::expected<mqss::QuantumResult, Error> executeQuantumTask(
    const mqss::QuantumTask &task, const mqss::submitter::Device &device,
    std::chrono::seconds waitTimeout, const std::atomic<bool> &terminationFlag);

/// @brief Cancel the quantum task and return a cancellation result.
/// @param task The quantum task to cancel.
/// @param cancelReason The reason for cancellation.
/// @return A QuantumResult indicating the task was cancelled, including the
/// cancellation reason.
[[nodiscard]] mqss::QuantumResult
cancelQuantumTask(const mqss::QuantumTask &task,
                  const std::string &cancelReason);

} // namespace mqss::qrmci
