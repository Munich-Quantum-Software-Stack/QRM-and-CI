/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Config.h"

#include "qrmci/ConfigDefaults.h"

// Non-throwing parse API (parse_result with operator bool()/.error()) rather
// than the exception-throwing default, matching this codebase's std::expected
// error-handling convention.
#define TOML_EXCEPTIONS 0
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <toml++/toml.hpp>

namespace mqss::qrmci {

namespace {

/// @brief The default relative path a config file is discovered at when
///        `QRMCI_CONFIG_FILE` is unset. Resolved against the process's
///        working directory, matching how each daemon is deployed in its own
///        container.
constexpr std::string_view kDefaultConfigFilePath = "config/qrmci.toml";

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

/// @brief Read a scalar value out of a TOML node, falling back when the node
///        is absent, of the wrong type, or the table itself was never parsed
///        (an empty root table, for a config file that does not exist).
/// @tparam Node `toml::node` or `toml::const node`, so this accepts a
///         node_view chained off either a mutable or (as `assembleConfig`
///         always passes) a `const toml::table&`.
template <class T, class Node>
T tomlValueOr(const toml::node_view<Node> &node, T fallback) {
  return node.template value<T>().value_or(fallback);
}

/// @brief Read a duration value stored in the file as a plain integer count
///        of @p Duration's own unit.
template <class Duration, class Node>
Duration tomlDurationOr(const toml::node_view<Node> &node, Duration fallback) {
  if (auto value = node.template value<int64_t>()) {
    return Duration(*value);
  }
  return fallback;
}

/// @brief Parse the TOML file at @p configFile, if it exists.
/// @return An empty table if @p configFile does not exist (config files are
///         optional), the parsed table on success, or a ConfigError if the
///         file exists but could not be parsed.
std::expected<toml::table, Error>
parseConfigFile(const std::filesystem::path &configFile) {
  std::error_code existsError;
  if (!std::filesystem::exists(configFile, existsError)) {
    return toml::table{};
  }

  toml::parse_result result = toml::parse_file(configFile.string());
  if (!result) {
    return std::unexpected(
        Error{Error::Kind::ConfigError,
              "Failed to parse config file '" + configFile.string() +
                  "': " + std::string(result.error().description())});
  }

  return std::move(result).table();
}

/// @brief Assemble a Config from a (possibly empty) parsed TOML table,
///        layering environment variable overrides on top for the fields that
///        carry a documented `QRMCI_*` env var contract. New tuning fields
///        introduced alongside the config file (timeouts, poll interval,
///        registry timing) are file-only and have no env var equivalent.
Config assembleConfig(const toml::table &root) {
  const auto &commonTable = root["common"];
  const auto &connectionTable = commonTable["connection"];
  const auto &selectorTable = root["selector"];
  const auto &compilerTable = root["compiler"];
  const auto &submitterTable = root["submitter"];

  const auto logDir =
      getEnvOr("QRMCI_LOG_DIR", tomlValueOr(commonTable["logDir"],
                                            std::string(defaults::LogDir)));

  Config config{
      .common =
          {
              .connection =
                  {
                      .host = getEnvOr(
                          "QRMCI_AMQP_HOST",
                          tomlValueOr(connectionTable["host"],
                                      std::string(defaults::AMPQHost))),
                      .port = getEnvOr("QRMCI_AMQP_PORT",
                                       tomlValueOr(connectionTable["port"],
                                                   defaults::AMPQPort)),
                      .user = getEnvOr(
                          "QRMCI_AMQP_USER",
                          tomlValueOr(connectionTable["user"],
                                      std::string(defaults::AMPQUser))),
                      .password = getEnvOr(
                          "QRMCI_AMQP_PASSWORD",
                          tomlValueOr(connectionTable["password"],
                                      std::string(defaults::AMPQPassword))),
                      .vhost = getEnvOr(
                          "QRMCI_AMQP_VHOST",
                          tomlValueOr(connectionTable["vhost"],
                                      std::string(defaults::AMPQVHost))),
                  },
              .logDir = logDir,
              .daemonLogger = tomlValueOr(commonTable["daemonLogger"],
                                          std::string("mqss::Daemon")),
              .daemonLog = logPath(logDir, defaults::DaemonLog),
              .qrmciQueue =
                  getEnvOr("QRMCI_QRMCI_QUEUE",
                           tomlValueOr(commonTable["qrmciQueue"],
                                       std::string(defaults::QRMCIQueue))),
              .resultsQueue =
                  getEnvOr("QRMCI_RESULTS_QUEUE",
                           tomlValueOr(commonTable["resultsQueue"],
                                       std::string(defaults::ResultsQueue))),
              .schedulerQueue =
                  getEnvOr("QRMCI_SCHEDULER_QUEUE",
                           tomlValueOr(commonTable["schedulerQueue"],
                                       std::string(defaults::SchedulerQueue))),
              .schedulerLogger = tomlValueOr(commonTable["schedulerLogger"],
                                             std::string("mqss::Scheduler")),
              .schedulerLog = logPath(logDir, defaults::SchedulerLog),
              .stagePollInterval =
                  tomlDurationOr(commonTable["stagePollInterval"],
                                 CommonConfig{}.stagePollInterval),
          },

      .selector =
          {
              .backendStatusQueue = getEnvOr(
                  "QRMCI_BACKEND_STATUS_QUEUE",
                  tomlValueOr(selectorTable["backendStatusQueue"],
                              std::string(defaults::BackendStatusQueue))),
              .logger = tomlValueOr(selectorTable["logger"],
                                    std::string("mqss::Selector")),
              .logFile = logPath(logDir, defaults::SelectorLog),
              .backendRegistry =
                  {
                      .entryTimeToLive = tomlDurationOr(
                          selectorTable["backendRegistry"]["entryTimeToLive"],
                          BackendRegistryConfig{}.entryTimeToLive),
                  },
              .backendStatusReceiveTimeout =
                  tomlDurationOr(selectorTable["backendStatusReceiveTimeout"],
                                 SelectorConfig{}.backendStatusReceiveTimeout),
              .taskReceiveTimeout =
                  tomlDurationOr(selectorTable["taskReceiveTimeout"],
                                 SelectorConfig{}.taskReceiveTimeout),
          },

      .compiler =
          {
              .queue =
                  getEnvOr("QRMCI_COMPILER_QUEUE",
                           tomlValueOr(compilerTable["queue"],
                                       std::string(defaults::CompilerQueue))),
              .logger = tomlValueOr(compilerTable["logger"],
                                    std::string("mqss::Compiler")),
              .logFile = logPath(logDir, defaults::CompilerLog),
              .taskReceiveTimeout =
                  tomlDurationOr(compilerTable["taskReceiveTimeout"],
                                 CompilerConfig{}.taskReceiveTimeout),
          },

      .submitter =
          {
              .queue =
                  getEnvOr("QRMCI_SUBMITTER_QUEUE",
                           tomlValueOr(submitterTable["queue"],
                                       std::string(defaults::SubmitterQueue))),
              .logger = tomlValueOr(submitterTable["logger"],
                                    std::string("mqss::Submitter")),
              .logFile = logPath(logDir, defaults::SubmitterLog),
              .qdmiDriverName =
                  getEnvOr("QRMCI_SUBMITTER_QDMI_DRIVER_NAME",
                           tomlValueOr(
                               submitterTable["qdmiDriverName"],
                               std::string(defaults::SubmitterQdmiDriverName))),
              .qdmiDeviceName =
                  getEnvOr("QRMCI_SUBMITTER_QDMI_DEVICE_NAME",
                           tomlValueOr(
                               submitterTable["qdmiDeviceName"],
                               std::string(defaults::SubmitterQdmiDeviceName))),
              .qdmiClientToken =
                  getEnvOr("QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN",
                           tomlValueOr(
                               submitterTable["qdmiClientToken"],
                               std::string(
                                   defaults::SubmitterQdmiClientToken))),
              .backendRegistry =
                  {
                      .entryTimeToLive =
                          tomlDurationOr(
                              submitterTable["backendRegistry"]
                                            ["entryTimeToLive"],
                              BackendRegistryConfig{}.entryTimeToLive),
                  },
              .backendStatusPublishInterval =
                  tomlDurationOr(
                      submitterTable["backendStatusPublishInterval"],
                      SubmitterConfig{}.backendStatusPublishInterval),
          },
  };

  return config;
}

} // namespace

std::expected<Config, Error>
loadConfig(const std::filesystem::path &configFile) {
  return parseConfigFile(configFile).transform([](const toml::table &root) {
    return assembleConfig(root);
  });
}

std::expected<Config, Error> loadConfig() {
  if (const char *envPath = std::getenv("QRMCI_CONFIG_FILE")) {
    return loadConfig(std::filesystem::path(envPath));
  }
  return loadConfig(std::filesystem::path(kDefaultConfigFilePath));
}

} // namespace mqss::qrmci
