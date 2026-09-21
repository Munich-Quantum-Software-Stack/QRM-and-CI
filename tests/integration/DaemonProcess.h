/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file DaemonProcess.h
/// @brief Owns one spawned QRM&CI daemon subprocess, for the
///        tests/integration binaries that need a live daemon running
///        against a real RabbitMQ/QDMI setup.

#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <sys/types.h>
#include <vector>

namespace mqss::qrmci::test {

/// @brief One environment variable to set (or overwrite) in a spawned
///        daemon's environment, layered on top of the parent process's own
///        environment.
struct EnvOverride {
  std::string name;
  std::string value;
};

/// @brief Owns one spawned daemon subprocess: starts it via `posix_spawn`
///        and stops it (SIGTERM, escalating to SIGKILL) on request or at
///        destruction. Move-only, since it owns a PID.
///
/// `posix_spawn` is used rather than `fork`+`exec`: the test binary links
/// `qrmci` (spdlog, the AMQP client), and `fork()` in a process with
/// background threads/locks risks the child deadlocking before `exec()`
/// runs. `posix_spawn` avoids duplicating that threaded state.
///
/// Ownership of the spawned PID ends the instant the child is known to have
/// exited, not merely when the caller happens to stop tracking it: every
/// path that can observe the exit (isRunning(), stop()'s own polling and
/// final blocking wait) clears the stored PID immediately, so an operation
/// issued afterward -- another stop(), another isRunning() -- has nothing
/// left to act on rather than risking a signal or wait aimed at whatever
/// unrelated process the OS may since have reused that PID for.
class DaemonProcess {
public:
  /// @param label Human-readable name used in diagnostics (e.g.
  ///        "qrmcid-standalone").
  /// @param executablePath Path to the daemon binary to spawn.
  /// @param logDir Directory the daemon's `QRMCI_LOG_DIR` is pointed at and
  ///        its stdout/stderr are captured under; created by start() if it
  ///        does not already exist.
  /// @param extraEnvOverrides Additional environment variables to set on top
  ///        of the parent process's own environment and `QRMCI_LOG_DIR`.
  DaemonProcess(std::string label, std::filesystem::path executablePath,
                std::filesystem::path logDir,
                std::vector<EnvOverride> extraEnvOverrides = {});

  DaemonProcess(const DaemonProcess &) = delete;
  DaemonProcess &operator=(const DaemonProcess &) = delete;
  DaemonProcess(DaemonProcess &&other) noexcept;
  DaemonProcess &operator=(DaemonProcess &&other) noexcept;

  /// @brief Best-effort stop() if the daemon is still running. Never throws.
  ~DaemonProcess();

  /// @brief Spawn the daemon.
  /// @return False on any setup or `posix_spawn` failure (logged to
  ///         stderr); the caller decides how to fail.
  [[nodiscard]] bool start();

  /// @brief Stop the daemon: SIGTERM, poll for exit up to @p gracePeriod,
  ///        then SIGKILL and a blocking wait. Idempotent -- a no-op once the
  ///        process is no longer running.
  void stop(std::chrono::milliseconds gracePeriod = std::chrono::seconds(5));

  /// @brief Non-blocking check: reaps the child if it has already exited,
  ///        clearing ownership (as if never started) the moment reaping
  ///        succeeds, rather than leaving the exited PID on record. Without
  ///        this, a later stop() acting on a stale PID risks signalling
  ///        whatever unrelated process the OS has since reused that number
  ///        for -- no claim is made about how likely that reuse is, only
  ///        that the object must not keep pointing at a PID it no longer
  ///        owns.
  /// @return True if the daemon is still running.
  [[nodiscard]] bool isRunning();

  /// @return The label passed at construction.
  [[nodiscard]] const std::string &getLabel() const { return label; }

  /// @return Where this daemon's stdout/stderr are captured.
  [[nodiscard]] std::filesystem::path getStdioLogPath() const {
    return logDir / "stdio.log";
  }

private:
  /// @brief Reap the child via a non-blocking waitpid() if it has exited.
  ///        Clears `pid` to -1 the moment waitpid reports the child gone --
  ///        either by returning its PID (reaped just now) or ECHILD (already
  ///        reaped elsewhere) -- so ownership never outlives the process it
  ///        names. Any other waitpid error is logged and ownership is left
  ///        in place, since it is not known whether the child is still
  ///        alive.
  /// @return True if the child is still running (ownership retained).
  bool reapIfExited();

  std::string label;
  std::filesystem::path executablePath;
  std::filesystem::path logDir;
  std::vector<EnvOverride> envOverrides;
  pid_t pid{-1};
};

} // namespace mqss::qrmci::test
