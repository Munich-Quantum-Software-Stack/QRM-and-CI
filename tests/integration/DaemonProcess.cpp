/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "DaemonProcess.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>

extern char **environ;

namespace mqss::qrmci::test {

namespace {

std::string envKey(const std::string &entry) {
  const auto pos = entry.find('=');
  return pos == std::string::npos ? entry : entry.substr(0, pos);
}

} // namespace

DaemonProcess::DaemonProcess(std::string label,
                             std::filesystem::path executablePath,
                             std::filesystem::path logDir,
                             std::vector<EnvOverride> extraEnvOverrides)
    : label(std::move(label)), executablePath(std::move(executablePath)),
      logDir(std::move(logDir)), envOverrides(std::move(extraEnvOverrides)) {}

DaemonProcess::DaemonProcess(DaemonProcess &&other) noexcept
    : label(std::move(other.label)),
      executablePath(std::move(other.executablePath)),
      logDir(std::move(other.logDir)),
      envOverrides(std::move(other.envOverrides)), pid(other.pid) {
  other.pid = -1;
}

DaemonProcess &DaemonProcess::operator=(DaemonProcess &&other) noexcept {
  if (this != &other) {
    stop();
    label = std::move(other.label);
    executablePath = std::move(other.executablePath);
    logDir = std::move(other.logDir);
    envOverrides = std::move(other.envOverrides);
    pid = other.pid;
    other.pid = -1;
  }
  return *this;
}

DaemonProcess::~DaemonProcess() { stop(); }

bool DaemonProcess::start() {
  std::error_code dirError;
  std::filesystem::create_directories(logDir, dirError);
  if (dirError) {
    std::cerr << label << ": could not create log directory " << logDir << ": "
              << dirError.message() << "\n";
    return false;
  }

  // Layer QRMCI_LOG_DIR on top of any caller-supplied overrides so every
  // spawned daemon logs into its own scratch directory instead of the
  // compiled-in default, which is not guaranteed to exist (or be writable)
  // outside the devcontainer's manual-start workflow.
  std::vector<EnvOverride> overrides = envOverrides;
  overrides.push_back(EnvOverride{"QRMCI_LOG_DIR", logDir.string()});

  // Start from the parent's own environment (so QDMI_CONF/LD_LIBRARY_PATH
  // set on this test binary's own ctest ENVIRONMENT reach the daemon too),
  // dropping any entry an override replaces -- glibc's getenv() returns the
  // first match, so a duplicate key appended after the original would be
  // silently ignored rather than winning.
  std::vector<std::string> envStrings;
  for (char **entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
    std::string current(*entry);
    const std::string key = envKey(current);
    const bool overridden =
        std::any_of(overrides.begin(), overrides.end(),
                    [&](const EnvOverride &ov) { return ov.name == key; });
    if (!overridden) {
      envStrings.push_back(std::move(current));
    }
  }
  for (const auto &ov : overrides) {
    envStrings.push_back(ov.name + "=" + ov.value);
  }

  std::vector<char *> envp;
  envp.reserve(envStrings.size() + 1);
  for (auto &entry : envStrings) {
    envp.push_back(entry.data());
  }
  envp.push_back(nullptr);

  const std::string execPathStr = executablePath.string();
  std::vector<char *> argv{const_cast<char *>(execPathStr.c_str()), nullptr};

  const auto stdioLog = getStdioLogPath();

  posix_spawn_file_actions_t fileActions;
  posix_spawn_file_actions_init(&fileActions);
  posix_spawn_file_actions_addopen(&fileActions, STDOUT_FILENO,
                                   stdioLog.c_str(),
                                   O_CREAT | O_WRONLY | O_TRUNC, 0644);
  posix_spawn_file_actions_adddup2(&fileActions, STDOUT_FILENO, STDERR_FILENO);

  pid_t spawnedPid = -1;
  const int rc = posix_spawn(&spawnedPid, execPathStr.c_str(), &fileActions,
                             nullptr, argv.data(), envp.data());
  posix_spawn_file_actions_destroy(&fileActions);

  if (rc != 0) {
    std::cerr << label << ": posix_spawn(" << execPathStr
              << ") failed: " << std::strerror(rc) << "\n";
    return false;
  }

  pid = spawnedPid;
  return true;
}

bool DaemonProcess::reapIfExited() {
  if (pid <= 0) {
    return false;
  }
  int status = 0;
  const pid_t result = waitpid(pid, &status, WNOHANG);
  if (result == 0) {
    return true;
  }
  if (result == pid || (result == -1 && errno == ECHILD)) {
    // Reaped just now, or already reaped by someone else: either way this
    // object no longer owns a live child, and must stop naming its old PID.
    pid = -1;
    return false;
  }
  // Some other waitpid failure: unclear whether the child is still alive,
  // so ownership is left in place for stop() to retry or report against.
  std::cerr << label << ": waitpid failed: " << std::strerror(errno) << "\n";
  return true;
}

bool DaemonProcess::isRunning() { return reapIfExited(); }

void DaemonProcess::stop(std::chrono::milliseconds gracePeriod) {
  if (pid <= 0) {
    return;
  }

  if (kill(pid, SIGTERM) != 0 && errno != ESRCH) {
    std::cerr << label << ": SIGTERM failed: " << std::strerror(errno) << "\n";
  }

  const auto deadline = std::chrono::steady_clock::now() + gracePeriod;
  bool stillRunning = isRunning();
  while (stillRunning && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stillRunning = isRunning();
  }

  if (stillRunning) {
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    pid = -1;
  }
}

} // namespace mqss::qrmci::test
