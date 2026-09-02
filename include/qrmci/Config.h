/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file Config.h
/// @brief Configuration model for the QRM workflow daemon.
///
/// This header defines the configuration structures used to assemble the
/// process-wide runtime configuration from defaults, an optional TOML file
/// and environment overrides. Fields are grouped by the pipeline stage that
/// consumes them (selector, compiler, submitter), plus a common group for
/// settings shared across every stage.

#pragma once

#include "qrmci/Error.h"

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>

namespace mqss::qrmci {

/// @brief RabbitMQ connection settings, shared by every stage's messaging.
struct RabbitMqConnectionConfig {
  std::string host;     ///< RabbitMQ host name.
  int port{};           ///< RabbitMQ TCP port.
  std::string user;     ///< RabbitMQ user name.
  std::string password; ///< RabbitMQ password.
  std::string vhost;    ///< RabbitMQ virtual host.
};

/// @brief How long a BackendRegistry entry stays usable after its last
///        refresh. Owned separately by whichever stage's registry it times,
///        since the selector's inbound registry and a submitter-owning
///        stage's own-status registry may reasonably need different values.
struct BackendRegistryConfig {
  /// @brief Entry time-to-live, forwarded to BackendRegistry's constructor.
  std::chrono::seconds entryTimeToLive{30};
};

/// @brief Settings shared by every pipeline stage: the RabbitMQ connection,
///        logging, the queues common to more than one stage (task intake,
///        results, and scheduling -- which never runs as its own distributed
///        process), and the daemon's outer poll loop cadence.
struct CommonConfig {
  RabbitMqConnectionConfig connection; ///< RabbitMQ connection settings.

  std::string logDir; ///< Directory where log files are written.

  std::string daemonLogger; ///< Logger name for the daemon.
  std::string daemonLog;    ///< Log file for the daemon.

  std::string qrmciQueue;   ///< Input queue for the QRM daemon.
  std::string resultsQueue; ///< Queue that carries execution results.

  std::string schedulerQueue;  ///< Queue used by the scheduler stage.
  std::string schedulerLogger; ///< Logger name for the scheduler.
  std::string schedulerLog;    ///< Log file for the scheduler.

  /// @brief Sleep between turns of a daemon's main loop. The one setting
  ///        with no single natural stage home, since the standalone daemon's
  ///        loop touches every group in one turn.
  std::chrono::milliseconds stagePollInterval{100};
};

/// @brief Backend-selector stage configuration: the distributed selector's
///        own queues, logging, and the timing of its own BackendRegistry
///        (which only ever absorbs published status -- it never publishes).
struct SelectorConfig {
  std::string backendStatusQueue; ///< Queue backend status is published on.
  std::string logger;             ///< Logger name for the selector.
  std::string logFile;            ///< Log file for the selector.

  BackendRegistryConfig backendRegistry; ///< This selector's registry timing.

  /// @brief Timeout for both of the selector's receive() calls (the
  ///        backend-status queue and the task-intake queue).
  std::chrono::milliseconds backendStatusReceiveTimeout{500};
  /// @brief Timeout for receiving the next task from the QRM&CI intake queue.
  std::chrono::milliseconds taskReceiveTimeout{500};
};

/// @brief Compiler-stage configuration: the queue a worker receives
///        already-selected tasks from, and its logging.
struct CompilerConfig {
  std::string queue;   ///< Queue used by the compiler stage.
  std::string logger;  ///< Logger name for the compiler.
  std::string logFile; ///< Log file for the compiler.

  /// @brief Timeout for the worker's receive() from this queue.
  std::chrono::milliseconds taskReceiveTimeout{500};
};

/// @brief Submitter-specific configuration: its own queue/logging, QDMI
///        credentials and device selection, and the timing of the
///        BackendRegistry owned by any stage that holds a submitter (the
///        distributed worker and the standalone daemon both source their
///        registry's timing from here, since both look up backends through
///        it for submission and publish their own status into it).
struct SubmitterConfig {
  std::string queue;   ///< Queue used by the submitter stage.
  std::string logger;  ///< Logger name for the submitter.
  std::string logFile; ///< Log file for the submitter.

  std::string qdmiDriverName;  ///< QDMI driver name.
  std::string qdmiDeviceName;  ///< QDMI device name.
  std::string qdmiClientToken; ///< QDMI client token.

  BackendRegistryConfig backendRegistry; ///< This submitter's registry timing.

  /// @brief Minimum spacing between two of this stage's own backend-status
  ///        publications.
  std::chrono::milliseconds backendStatusPublishInterval{5000};
};

/// @brief Aggregate configuration for the QRM daemon, grouped by the
///        pipeline stage that consumes each field.
struct Config {
  CommonConfig common;       ///< Settings shared by every stage.
  SelectorConfig selector;   ///< Backend-selector stage configuration.
  CompilerConfig compiler;   ///< Compiler stage configuration.
  SubmitterConfig submitter; ///< Submitter stage configuration.
};

/// @brief Build a configuration from defaults, an optional TOML file and
///        environment overrides.
///
/// Precedence is defaults < file < environment variables. The file is
/// discovered from the `QRMCI_CONFIG_FILE` environment variable if set,
/// otherwise from the relative default path `config/qrmci.toml`; a missing
/// file is not an error and simply leaves the corresponding fields at their
/// defaults/environment values. A file that exists but fails to parse is
/// reported as `Error::Kind::ConfigError`.
/// @return The assembled configuration, or a ConfigError describing why the
///         discovered file could not be read.
std::expected<Config, Error> loadConfig();

/// @brief Build a configuration from defaults, the TOML file at @p
///        configFile, and environment overrides, bypassing the usual
///        `QRMCI_CONFIG_FILE`/default-path discovery.
/// @param configFile The TOML file to load. A missing file is not an error.
/// @return The assembled configuration, or a ConfigError describing why @p
///         configFile could not be read.
std::expected<Config, Error>
loadConfig(const std::filesystem::path &configFile);

} // namespace mqss::qrmci
