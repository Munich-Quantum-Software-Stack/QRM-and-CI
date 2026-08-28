/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Config.h"

#include "qrmci/ConfigDefaults.h"

#include <atomic>
#include <cassert>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace mqss::qrmci {

namespace {

Config &mutableConfig() {
  static Config config;
  return config;
}

std::atomic<bool> &configInitialized() {
  static std::atomic<bool> initialized = false;
  return initialized;
}

std::string getEnvOr(const char *name, std::string_view fallback) {
  if (const char *value = std::getenv(name)) {
    return value;
  }

  return std::string(fallback);
}

int getEnvOr(const char *name, int fallback) {
  if (const char *value = std::getenv(name)) {
    int result = fallback;
    const std::string_view sv(value);

    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
    if (ec == std::errc{} && ptr == sv.data() + sv.size()) {
      return result;
    }
  }

  return fallback;
}

std::string logPath(std::string_view logDir, std::string_view fileName) {
  return (std::filesystem::path(logDir) / fileName).string();
}

} // namespace

Config loadConfig(int argc, char **argv) {
  const auto logDir = getEnvOr("QRM_LOG_DIR", defaults::LogDir);

  Config config{
      .rabbitmq =
          {
              .host = getEnvOr("QRM_AMQP_HOST", defaults::AMPQHost),
              .port = getEnvOr("QRM_AMQP_PORT", defaults::AMPQPort),
              .user = getEnvOr("QRM_AMQP_USER", defaults::AMPQUser),
              .password = getEnvOr("QRM_AMQP_PASSWORD", defaults::AMPQPassword),
              .vhost = getEnvOr("QRM_AMQP_VHOST", defaults::AMPQVHost),
          },

      .queues =
          {
              .qrmci = getEnvOr("QRM_QRMCI_QUEUE", defaults::QRMCIQueue),
              .scheduler =
                  getEnvOr("QRM_SCHEDULER_QUEUE", defaults::SchedulerQueue),
              .compiler =
                  getEnvOr("QRM_COMPILER_QUEUE", defaults::CompilerQueue),
              .results = getEnvOr("QRM_RESULTS_QUEUE", defaults::ResultsQueue),
              .submitter =
                  getEnvOr("QRM_SUBMITTER_QUEUE", defaults::SubmitterQueue),
          },

      .logging =
          {
              .logDir = logDir,

              .daemonLogger = "mqss::Daemon",
              .schedulerLogger = "mqss::Scheduler",
              .compilerLogger = "mqss::Compiler",
              .submitterLogger = "mqss::Submitter",

              .daemonLog = logPath(logDir, "daemon.log"),
              .schedulerLog = logPath(logDir, "scheduler.log"),
              .compilerLog = logPath(logDir, "compiler.log"),
              .submitterLog = logPath(logDir, "submitter.log"),
          },
      .submitter =
          {
              .qdmiDriverName =
                  getEnvOr("SUBMITTER_QDMI_DRIVER_NAME", "qdmi_example_driver"),
              .qdmiDeviceName = getEnvOr("SUBMITTER_QDMI_DEVICE_NAME",
                                         "C++ Device with 5 qubits"),
              .qdmiClientToken =
                  getEnvOr("SUBMITTER_QDMI_CLIENT_TOKEN", "token"),
          },
  };

  // Integrate CLI11 here when runtime CLI overrides are required.
  (void)argc;
  (void)argv;

  return config;
}

void initConfig(Config newConfig) {
  assert(!configInitialized() &&
         "QRM workflow configuration already initialized");

  mutableConfig() = std::move(newConfig);
  configInitialized() = true;
}

const Config &getConfig() {
  assert(configInitialized() && "QRM workflow configuration not initialized");

  return mutableConfig();
}

} // namespace mqss::qrmci
