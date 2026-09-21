/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Config.h"

#include "qrmci/BackendRegistry.h"
#include "qrmci/ConfigDefaults.h"
#include "qrmci/Error.h"

// TOML_EXCEPTIONS=0 (non-throwing parse API: parse_result with
// operator bool()/.error(), matching this codebase's std::expected
// error-handling convention) is set target-wide in CMakeLists.txt so every
// qrmci translation unit that includes toml++ agrees on the ABI.
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <toml++/toml.hpp>
#include <utility>

namespace mqss::qrmci {

namespace {

/// @brief The default relative path a config file is discovered at when
///        `QRMCI_CONFIG_FILE` is unset. Resolved against the process's
///        working directory, matching how each daemon is deployed in its own
///        container.
constexpr std::string_view DefaultConfigFilePath = "config/qrmci.toml";

std::string getEnvOr(const char *name, std::string_view fallback) {
  if (const char *value = std::getenv(name)) {
    return value;
  }

  return std::string(fallback);
}

/// @brief Parse a required-integer environment variable, failing on a
///        present-but-malformed value rather than silently keeping @p
///        fallback -- a typo'd deployment override should not masquerade as
///        an accepted default.
std::expected<int, Error> getEnvIntOrExpected(const char *name, int fallback) {
  const char *value = std::getenv(name);
  if (value == nullptr) {
    return fallback;
  }

  int result = 0;
  const std::string_view sv(value);
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
  if (ec == std::errc{} && ptr == sv.data() + sv.size()) {
    return result;
  }

  return std::unexpected(Error{Error::Kind::ConfigError,
                               "Environment variable '" + std::string(name) +
                                   "' is not a valid integer"});
}

std::string logPath(std::string_view logDir, std::string_view fileName) {
  return (std::filesystem::path(logDir) / fileName).string();
}

/// @brief Read a scalar value out of a TOML node, failing when the node is
///        present but the wrong type, and falling back only when the node
///        (or its enclosing table) is absent entirely.
template <class T>
std::expected<T, Error>
tomlValueOrExpected(const toml::node_view<const toml::node> &node,
                    std::string_view dottedKey, T fallback) {
  if (!node) {
    return fallback;
  }
  if (auto value = node.value<T>()) {
    return std::move(*value);
  }
  return std::unexpected(Error{Error::Kind::ConfigError,
                               "Configuration key '" + std::string(dottedKey) +
                                   "' has the wrong type"});
}

enum class DurationRange { Positive, NonNegative };

/// @brief Read and validate an integer count of @p Duration's own unit,
///        using @p fallback only when the node is absent.
template <class Duration>
std::expected<Duration, Error>
tomlDurationOrExpected(const toml::node_view<const toml::node> &node,
                       std::string_view dottedKey, Duration fallback,
                       DurationRange range) {
  auto duration = fallback;
  if (node) {
    const auto value = node.value<int64_t>();
    if (!value) {
      return std::unexpected(
          Error{Error::Kind::ConfigError,
                "Configuration key '" + std::string(dottedKey) +
                    "' has the wrong type: expected an integer"});
    }
    duration = Duration(*value);
  }
  if (duration < Duration::zero() ||
      (range == DurationRange::Positive && duration == Duration::zero())) {
    return std::unexpected(Error{
        Error::Kind::ConfigError,
        "Configuration key '" + std::string(dottedKey) +
            (range == DurationRange::Positive ? "' must be greater than zero"
                                              : "' must not be negative")});
  }
  return duration;
}

/// @brief A string field read from the file, then overridden by @p envName
///        if set. Fails only on a present-but-wrong-type file value; an
///        env var is always a valid string.
std::expected<std::string, Error>
stringField(const toml::node_view<const toml::node> &node,
            std::string_view dottedKey, const char *envName,
            std::string_view fallback) {
  return tomlValueOrExpected<std::string>(node, dottedKey,
                                          std::string(fallback))
      .transform([&](const std::string &fileValue) {
        return getEnvOr(envName, fileValue);
      });
}

/// @brief Like @ref stringField, additionally rejecting an empty final value
///        (after defaults/file/env are all resolved) -- these fields name a
///        queue or a filesystem path, and an empty one can never work.
std::expected<std::string, Error>
requiredStringField(const toml::node_view<const toml::node> &node,
                    std::string_view dottedKey, const char *envName,
                    std::string_view fallback) {
  return stringField(node, dottedKey, envName, fallback)
      .and_then([&](std::string value) -> std::expected<std::string, Error> {
        if (value.empty()) {
          return std::unexpected(
              Error{Error::Kind::ConfigError, "Configuration key '" +
                                                  std::string(dottedKey) +
                                                  "' must not be empty"});
        }
        return value;
      });
}

/// @brief Parse `[selector] selectionPolicy`'s two exact kebab-case values.
///        Intentionally strict (no case-folding, no aliases) so a typo fails
///        loudly instead of silently picking a policy the deployer did not
///        choose.
std::expected<BackendSelectionPolicy, Error>
parseSelectionPolicy(std::string_view value, std::string_view dottedKey) {
  if (value == "lowest-name") {
    return BackendSelectionPolicy::LowestName;
  }
  if (value == "smallest-sufficient") {
    return BackendSelectionPolicy::SmallestSufficient;
  }
  return std::unexpected(
      Error{Error::Kind::ConfigError,
            "Configuration key '" + std::string(dottedKey) +
                "' must be 'lowest-name' or 'smallest-sufficient'"});
}

/// @brief Reject a table node with any key outside @p allowedKeys, naming
///        the first offender with its full dotted path. A present node that
///        is not a table at all is itself rejected.
std::expected<void, Error>
checkTableKeys(const toml::node_view<const toml::node> &tableNode,
               std::string_view tablePrefix,
               std::initializer_list<std::string_view> allowedKeys) {
  if (!tableNode) {
    return {};
  }
  const auto *table = tableNode.as_table();
  if (table == nullptr) {
    return std::unexpected(
        Error{Error::Kind::ConfigError,
              "Configuration key '" +
                  std::string(tablePrefix.substr(0, tablePrefix.size() - 1)) +
                  "' must be a table"});
  }
  for (auto &&[key, value] : *table) {
    (void)value;
    const std::string_view keyView = key.str();
    if (!std::ranges::contains(allowedKeys, keyView)) {
      return std::unexpected(
          Error{Error::Kind::ConfigError, "Unknown configuration key '" +
                                              std::string(tablePrefix) +
                                              std::string(keyView) + "'"});
    }
  }
  return {};
}

/// @brief Reject any top-level or documented-nested key this build does not
///        know about (typos, stale/renamed fields), before assembling a
///        Config from what remains. Keeping this walk separate from
///        `assembleConfig()` means adding a genuinely new field is exactly
///        two edits: the allowed-key list here, and the field's own read.
std::expected<void, Error> checkSchema(const toml::table &root) {
  if (auto result =
          checkTableKeys(toml::node_view{root}, "",
                         {"common", "selector", "compiler", "submitter"});
      !result) {
    return result;
  }

  if (auto result = checkTableKeys(
          root["common"], "common.",
          {"connection", "logDir", "daemonLogger", "qrmciQueue", "resultsQueue",
           "schedulerQueue", "schedulerLogger", "stagePollInterval"});
      !result) {
    return result;
  }
  if (auto result =
          checkTableKeys(root["common"]["connection"], "common.connection.",
                         {"host", "port", "user", "password", "vhost"});
      !result) {
    return result;
  }
  if (auto result = checkTableKeys(root["selector"], "selector.",
                                   {"backendStatusQueue", "logger",
                                    "backendRegistry", "selectionPolicy"});
      !result) {
    return result;
  }
  if (auto result =
          checkTableKeys(root["selector"]["backendRegistry"],
                         "selector.backendRegistry.", {"entryTimeToLive"});
      !result) {
    return result;
  }
  if (auto result = checkTableKeys(root["compiler"], "compiler.",
                                   {"queue", "logger", "taskReceiveTimeout"});
      !result) {
    return result;
  }
  if (auto result =
          checkTableKeys(root["submitter"], "submitter.",
                         {"queue", "logger", "qdmiDriver", "qdmiDeviceName",
                          "qdmiDeviceId", "qdmiClientToken", "jobWaitTimeout",
                          "backendRegistry", "backendStatusPublishInterval"});
      !result) {
    return result;
  }
  return checkTableKeys(root["submitter"]["backendRegistry"],
                        "submitter.backendRegistry.", {"entryTimeToLive"});
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
///        carry a documented `QRMCI_*` contract.
///
/// Connection settings, queue names, the log directory and the QDMI target
/// are overridable from the environment, because those are what differs
/// between deployments of the same image. The tuning fields -- timeouts, poll
/// interval, registry timing -- are file-only.
///
/// Every field is validated for type, and the fields that must never be
/// empty, non-positive, or negative are checked after defaults/file/env are
/// all resolved, so `Error::detail` always names the exact dotted key at
/// fault -- regardless of which layer supplied the bad value.
std::expected<Config, Error> assembleConfig(const toml::table &root) {
  if (auto schemaResult = checkSchema(root); !schemaResult) {
    return std::unexpected(std::move(schemaResult.error()));
  }

  const auto &commonTable = root["common"];
  const auto &connectionTable = commonTable["connection"];
  const auto &selectorTable = root["selector"];
  const auto &compilerTable = root["compiler"];
  const auto &submitterTable = root["submitter"];

  auto logDir = stringField(commonTable["logDir"], "common.logDir",
                            "QRMCI_LOG_DIR", defaults::LogDir);
  if (!logDir) {
    return std::unexpected(std::move(logDir.error()));
  }

  auto host =
      requiredStringField(connectionTable["host"], "common.connection.host",
                          "QRMCI_AMQP_HOST", defaults::AMQPHost);
  if (!host) {
    return std::unexpected(std::move(host.error()));
  }

  auto port =
      tomlValueOrExpected<int>(connectionTable["port"],
                               "common.connection.port", defaults::AMQPPort)
          .and_then([](int fileValue) {
            return getEnvIntOrExpected("QRMCI_AMQP_PORT", fileValue);
          });
  if (!port) {
    return std::unexpected(std::move(port.error()));
  }
  if (*port < 1 || *port > 65535) {
    return std::unexpected(
        Error{Error::Kind::ConfigError,
              "Configuration key 'common.connection.port' must be between 1 "
              "and 65535"});
  }

  auto user = stringField(connectionTable["user"], "common.connection.user",
                          "QRMCI_AMQP_USER", defaults::AMQPUser);
  if (!user) {
    return std::unexpected(std::move(user.error()));
  }

  auto password =
      stringField(connectionTable["password"], "common.connection.password",
                  "QRMCI_AMQP_PASSWORD", defaults::AMQPPassword);
  if (!password) {
    return std::unexpected(std::move(password.error()));
  }

  auto vhost = stringField(connectionTable["vhost"], "common.connection.vhost",
                           "QRMCI_AMQP_VHOST", defaults::AMQPVHost);
  if (!vhost) {
    return std::unexpected(std::move(vhost.error()));
  }

  auto daemonLogger = tomlValueOrExpected<std::string>(
      commonTable["daemonLogger"], "common.daemonLogger",
      std::string("mqss::Daemon"));
  if (!daemonLogger) {
    return std::unexpected(std::move(daemonLogger.error()));
  }

  auto qrmciQueue =
      requiredStringField(commonTable["qrmciQueue"], "common.qrmciQueue",
                          "QRMCI_QRMCI_QUEUE", defaults::QRMCIQueue);
  if (!qrmciQueue) {
    return std::unexpected(std::move(qrmciQueue.error()));
  }

  auto resultsQueue =
      stringField(commonTable["resultsQueue"], "common.resultsQueue",
                  "QRMCI_RESULTS_QUEUE", defaults::ResultsQueue);
  if (!resultsQueue) {
    return std::unexpected(std::move(resultsQueue.error()));
  }

  auto schedulerQueue =
      stringField(commonTable["schedulerQueue"], "common.schedulerQueue",
                  "QRMCI_SCHEDULER_QUEUE", defaults::SchedulerQueue);
  if (!schedulerQueue) {
    return std::unexpected(std::move(schedulerQueue.error()));
  }

  auto schedulerLogger = tomlValueOrExpected<std::string>(
      commonTable["schedulerLogger"], "common.schedulerLogger",
      std::string("mqss::Scheduler"));
  if (!schedulerLogger) {
    return std::unexpected(std::move(schedulerLogger.error()));
  }

  auto stagePollInterval = tomlDurationOrExpected<std::chrono::milliseconds>(
      commonTable["stagePollInterval"], "common.stagePollInterval",
      CommonConfig{}.stagePollInterval, DurationRange::Positive);
  if (!stagePollInterval) {
    return std::unexpected(std::move(stagePollInterval.error()));
  }

  auto backendStatusQueue = requiredStringField(
      selectorTable["backendStatusQueue"], "selector.backendStatusQueue",
      "QRMCI_BACKEND_STATUS_QUEUE", defaults::BackendStatusQueue);
  if (!backendStatusQueue) {
    return std::unexpected(std::move(backendStatusQueue.error()));
  }

  auto selectorLogger = tomlValueOrExpected<std::string>(
      selectorTable["logger"], "selector.logger",
      std::string("mqss::Selector"));
  if (!selectorLogger) {
    return std::unexpected(std::move(selectorLogger.error()));
  }

  auto selectorEntryTimeToLive = tomlDurationOrExpected<std::chrono::seconds>(
      selectorTable["backendRegistry"]["entryTimeToLive"],
      "selector.backendRegistry.entryTimeToLive",
      BackendRegistryConfig{}.entryTimeToLive, DurationRange::Positive);
  if (!selectorEntryTimeToLive) {
    return std::unexpected(std::move(selectorEntryTimeToLive.error()));
  }

  auto selectionPolicy =
      tomlValueOrExpected<std::string>(selectorTable["selectionPolicy"],
                                       "selector.selectionPolicy",
                                       std::string("lowest-name"))
          .and_then([](const std::string &value) {
            return parseSelectionPolicy(value, "selector.selectionPolicy");
          });
  if (!selectionPolicy) {
    return std::unexpected(std::move(selectionPolicy.error()));
  }

  auto compilerQueue =
      requiredStringField(compilerTable["queue"], "compiler.queue",
                          "QRMCI_COMPILER_QUEUE", defaults::CompilerQueue);
  if (!compilerQueue) {
    return std::unexpected(std::move(compilerQueue.error()));
  }

  auto compilerLogger = tomlValueOrExpected<std::string>(
      compilerTable["logger"], "compiler.logger",
      std::string("mqss::Compiler"));
  if (!compilerLogger) {
    return std::unexpected(std::move(compilerLogger.error()));
  }

  auto taskReceiveTimeout = tomlDurationOrExpected<std::chrono::milliseconds>(
      compilerTable["taskReceiveTimeout"], "compiler.taskReceiveTimeout",
      CompilerConfig{}.taskReceiveTimeout, DurationRange::NonNegative);
  if (!taskReceiveTimeout) {
    return std::unexpected(std::move(taskReceiveTimeout.error()));
  }

  auto submitterQueue =
      stringField(submitterTable["queue"], "submitter.queue",
                  "QRMCI_SUBMITTER_QUEUE", defaults::SubmitterQueue);
  if (!submitterQueue) {
    return std::unexpected(std::move(submitterQueue.error()));
  }

  auto submitterLogger = tomlValueOrExpected<std::string>(
      submitterTable["logger"], "submitter.logger",
      std::string("mqss::Submitter"));
  if (!submitterLogger) {
    return std::unexpected(std::move(submitterLogger.error()));
  }

  auto qdmiDriver = requiredStringField(
      submitterTable["qdmiDriver"], "submitter.qdmiDriver",
      "QRMCI_SUBMITTER_QDMI_DRIVER", defaults::SubmitterqdmiDriver);
  if (!qdmiDriver) {
    return std::unexpected(std::move(qdmiDriver.error()));
  }

  auto qdmiDeviceName = requiredStringField(
      submitterTable["qdmiDeviceName"], "submitter.qdmiDeviceName",
      "QRMCI_SUBMITTER_QDMI_DEVICE_NAME", defaults::SubmitterQdmiDeviceName);
  if (!qdmiDeviceName) {
    return std::unexpected(std::move(qdmiDeviceName.error()));
  }

  auto qdmiDeviceId = requiredStringField(
      submitterTable["qdmiDeviceId"], "submitter.qdmiDeviceId",
      "QRMCI_SUBMITTER_QDMI_DEVICE_ID", defaults::SubmitterQdmiDeviceId);
  if (!qdmiDeviceId) {
    return std::unexpected(std::move(qdmiDeviceId.error()));
  }

  auto qdmiClientToken = stringField(
      submitterTable["qdmiClientToken"], "submitter.qdmiClientToken",
      "QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN", defaults::SubmitterQdmiClientToken);
  if (!qdmiClientToken) {
    return std::unexpected(std::move(qdmiClientToken.error()));
  }

  auto jobWaitTimeout = tomlDurationOrExpected<std::chrono::seconds>(
      submitterTable["jobWaitTimeout"], "submitter.jobWaitTimeout",
      SubmitterConfig{}.jobWaitTimeout, DurationRange::NonNegative);
  if (!jobWaitTimeout) {
    return std::unexpected(std::move(jobWaitTimeout.error()));
  }

  auto submitterEntryTimeToLive = tomlDurationOrExpected<std::chrono::seconds>(
      submitterTable["backendRegistry"]["entryTimeToLive"],
      "submitter.backendRegistry.entryTimeToLive",
      BackendRegistryConfig{}.entryTimeToLive, DurationRange::Positive);
  if (!submitterEntryTimeToLive) {
    return std::unexpected(std::move(submitterEntryTimeToLive.error()));
  }

  auto backendStatusPublishInterval =
      tomlDurationOrExpected<std::chrono::milliseconds>(
          submitterTable["backendStatusPublishInterval"],
          "submitter.backendStatusPublishInterval",
          SubmitterConfig{}.backendStatusPublishInterval,
          DurationRange::Positive);
  if (!backendStatusPublishInterval) {
    return std::unexpected(std::move(backendStatusPublishInterval.error()));
  }

  // The submitter's own registry TTL must outlive the interval between its
  // own status publications, or a freshly-published entry could expire
  // before the next publication refreshes it. Chrono compares both durations
  // in their common unit (milliseconds).
  if (*submitterEntryTimeToLive <= *backendStatusPublishInterval) {
    return std::unexpected(Error{
        Error::Kind::ConfigError,
        "Configuration key 'submitter.backendRegistry.entryTimeToLive' must "
        "be greater than 'submitter.backendStatusPublishInterval'"});
  }

  const auto daemonLog = logPath(*logDir, defaults::DaemonLog);
  const auto schedulerLog = logPath(*logDir, defaults::SchedulerLog);
  const auto selectorLog = logPath(*logDir, defaults::SelectorLog);
  const auto compilerLog = logPath(*logDir, defaults::CompilerLog);
  const auto submitterLog = logPath(*logDir, defaults::SubmitterLog);

  return Config{
      .common =
          {
              .connection =
                  {
                      .host = std::move(*host),
                      .port = *port,
                      .user = std::move(*user),
                      .password = std::move(*password),
                      .vhost = std::move(*vhost),
                  },
              .logDir = std::move(*logDir),
              .daemonLogger = std::move(*daemonLogger),
              .daemonLog = daemonLog,
              .qrmciQueue = std::move(*qrmciQueue),
              .resultsQueue = std::move(*resultsQueue),
              .schedulerQueue = std::move(*schedulerQueue),
              .schedulerLogger = std::move(*schedulerLogger),
              .schedulerLog = schedulerLog,
              .stagePollInterval = *stagePollInterval,
          },

      .selector =
          {
              .backendStatusQueue = std::move(*backendStatusQueue),
              .logger = std::move(*selectorLogger),
              .logFile = selectorLog,
              .backendRegistry = {.entryTimeToLive = *selectorEntryTimeToLive},
              .selectionPolicy = *selectionPolicy,
          },

      .compiler =
          {
              .queue = std::move(*compilerQueue),
              .logger = std::move(*compilerLogger),
              .logFile = compilerLog,
              .taskReceiveTimeout = *taskReceiveTimeout,
          },

      .submitter =
          {
              .queue = std::move(*submitterQueue),
              .logger = std::move(*submitterLogger),
              .logFile = submitterLog,
              .qdmiDriver = std::move(*qdmiDriver),
              .qdmiDeviceName = std::move(*qdmiDeviceName),
              .qdmiDeviceId = std::move(*qdmiDeviceId),
              .qdmiClientToken = std::move(*qdmiClientToken),
              .jobWaitTimeout = *jobWaitTimeout,
              .backendRegistry = {.entryTimeToLive = *submitterEntryTimeToLive},
              .backendStatusPublishInterval = *backendStatusPublishInterval,
          },
  };
}

} // namespace

std::expected<Config, Error>
loadConfig(const std::filesystem::path &configFile) {
  return parseConfigFile(configFile).and_then(assembleConfig);
}

std::expected<Config, Error> loadConfig() {
  return loadConfig(getEnvOr("QRMCI_CONFIG_FILE", DefaultConfigFilePath));
}

} // namespace mqss::qrmci
