/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file Daemon.h
/// @brief The scaffolding every QRM&CI daemon entrypoint shares: process
///        bring-up, opening the configured QDMI device, and the
///        compile-and-schedule stage.

#pragma once

#include "qrmci/BackendRegistry.h"
#include "qrmci/Config.h"
#include "qrmci/Error.h"
#include "qrmci/PublicationThrottle.h"
#include "scheduler/scheduler.hpp"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <atomic>
#include <csignal>
#include <expected>
#include <mqss/submitter/Device.h>
#include <optional>
#include <string_view>

namespace mqss::qrmci {

/// @brief The scheduler every daemon that submits work runs its tasks
///        through.
using TaskScheduler = mqss::scheduler::Scheduler<mqss::QuantumTask>;

/// @brief Set to true once SIGINT or SIGTERM has been received. A daemon's
///        main loop and CommunicationHandler::receive() both consult this to
///        unwind promptly instead of finishing whatever bounded wait or
///        drain they were already in.
inline constinit std::atomic<bool> terminationRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

/// @brief Register SIGINT and SIGTERM handlers that set terminationRequested.
///        Called by initializeDaemon(), before constructing anything that
///        should observe a clean shutdown.
inline void installTerminationHandlers() {
  const auto handler = [](int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
      terminationRequested.store(true, std::memory_order_relaxed);
    }
  };
  std::signal(SIGINT, handler);
  std::signal(SIGTERM, handler);
}

/// @brief Bring a daemon process up: install the SIGINT/SIGTERM handlers,
///        assemble the configuration, and make the daemon logger the
///        process-wide default.
///
/// @return The assembled configuration, or the ConfigError explaining why it
///         could not be assembled. Nothing is logged and no logger is
///         installed on failure; the caller reports it and exits.
[[nodiscard]] std::expected<Config, Error> initializeDaemon();

/// @brief Open the QDMI device @p config names.
///
/// The driver's QDMI version is detected from the driver itself, so nothing
/// about it is declared here. An empty client token or device ID in the
/// configuration means "unset" rather than "empty string", which is the one
/// subtlety this wrapper exists to keep in a single place.
/// @param config The submitter configuration naming the driver and device.
/// @return The opened device, or a DriverUnavailable error explaining why it
///         could not be opened.
[[nodiscard]] std::expected<mqss::submitter::Device, Error>
openConfiguredDevice(const SubmitterConfig &config);

/// @brief Read this process's own device and register it, so the daemon has
///        a backend to schedule onto before its work loop starts.
/// @param device The device to read.
/// @param backends The registry to insert the device's status into.
/// @param dispatchQueue The queue tasks assigned to this backend should be
///        sent to, forwarded to BackendWrapper::fromSubmitter(). Left empty
///        for a process that never publishes its own status.
/// @return Nothing on success, or the DeviceError explaining why the device
///         could not be read -- a daemon has nothing to offer if it cannot
///         read its own device once, so the caller exits on failure.
[[nodiscard]] std::expected<void, Error>
registerOwnDevice(const mqss::submitter::Device &device,
                  BackendRegistry &backends,
                  std::string_view dispatchQueue = "");

/// @brief Re-read this process's own device and refresh its registry entry,
///        if @p throttle has a publish slot due.
///
/// A device that cannot be read is a warning, not a fatal error: the entry's
/// own time-to-live already governs how long a stale snapshot may stand, and
/// a device that recovers is picked up on the next slot. Nothing is
/// published from here either -- a caller that publishes its status does so
/// with the returned snapshot, so a failing device says nothing at all
/// rather than announcing itself healthy.
/// @param device The device to re-read.
/// @param backends The registry holding this process's own entry.
/// @param throttle The throttle pacing this process's own publication slots.
/// @param dispatchQueue The queue tasks assigned to this backend should be
///        sent to, forwarded to BackendWrapper::fromSubmitter(). Left empty
///        for a process that never publishes its own status.
/// @return The refreshed status to publish, or nullopt if no slot was due or
///         the device could not be read.
std::optional<mqss::Backend>
refreshOwnDeviceStatus(const mqss::submitter::Device &device,
                       BackendRegistry &backends, PublicationThrottle &throttle,
                       std::string_view dispatchQueue = "");

/// @brief Log every registered backend and its capabilities at info level.
/// @param backends The registry to list.
void logRegisteredBackends(const BackendRegistry &backends);

/// @brief Select a backend for @p task, compile it for that backend, and
///        hand it to @p scheduler.
///
/// The standalone daemon's whole intake stage. @p task is left with its
/// scheduled QPU assigned and its circuits compiled on success, and
/// untouched past the point of failure otherwise.
/// @param task The task to prepare, mutated in place.
/// @param backends The backends to choose from.
/// @param compiler The compiler to compile with.
/// @param scheduler The scheduler to hand the compiled task to.
/// @param policy The rule for picking among several compatible backends.
/// @return Nothing on success, or the error from whichever step failed.
[[nodiscard]] std::expected<void, Error> selectCompileAndSchedule(
    mqss::QuantumTask &task, const BackendRegistry &backends,
    mqss::mqssci::MQSSCompiler &compiler, TaskScheduler &scheduler,
    BackendSelectionPolicy policy = BackendSelectionPolicy::LowestName);

/// @brief Compile @p task for the backend it was already assigned and hand
///        it to @p scheduler.
///
/// @param task The task to compile, mutated in place. Its scheduled QPU
///        names the backend to compile for.
/// @param backends The registry the scheduled QPU is looked up in.
/// @param compiler The compiler to compile with.
/// @param scheduler The scheduler to hand the compiled task to.
/// @return Nothing on success, a NoBackendAvailable error if the scheduled
///         QPU is not registered with this process, or the compiler's own
///         error.
[[nodiscard]] std::expected<void, Error>
compileAndSchedule(mqss::QuantumTask &task, const BackendRegistry &backends,
                   mqss::mqssci::MQSSCompiler &compiler,
                   TaskScheduler &scheduler);

} // namespace mqss::qrmci
