/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file Config.h
/// @brief The runtime configuration model shared by every QRM&CI daemon.
///
/// A configuration is assembled from compiled-in defaults, an optional TOML
/// file and environment overrides, in that order of increasing precedence.
/// Fields are grouped by the pipeline responsibility that owns them
/// (selector, compiler, submitter), plus a common group for settings shared
/// across every stage. The grouping names which stage's concern a field is,
/// not which process is allowed to read it: a distributed process reads
/// every group whose responsibility its own work touches, not only the one
/// named after it. For example the distributed worker reads `[submitter]`
/// for its own device and registry timing but also `[selector]`'s
/// `backendStatusQueue` to publish its status, and `[compiler]` for the
/// queue it receives tasks on -- see Config below for the exact reads of
/// each shipped entrypoint.

#pragma once

#include "qrmci/BackendRegistry.h"
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

  std::string qrmciQueue;   ///< Queue incoming tasks arrive on.
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

  /// @brief The rule for picking among several compatible backends, driving
  ///        both the standalone daemon's and the distributed selector's own
  ///        selection. File-only (`[selector] selectionPolicy`, one of
  ///        `"lowest-name"` or `"smallest-sufficient"`); an unrecognized
  ///        string is a ConfigError, not a silent fallback.
  BackendSelectionPolicy selectionPolicy{BackendSelectionPolicy::LowestName};
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

  /// @brief Filesystem path to the QDMI driver library to load. Resolved by
  ///        Client::openDevice(), which requires the file to exist -- a bare
  ///        library stem is not searched for on the library path.
  std::string qdmiDriver;
  std::string qdmiDeviceName;  ///< QDMI device name.
  std::string qdmiDeviceId;    ///< QDMI device id.
  std::string qdmiClientToken; ///< QDMI client token.

  /// @brief How long collectQuantumResult() waits for each submitted job to
  ///        reach a final state. Zero waits indefinitely, matching QDMI's own
  ///        zero-means-infinite convention. File-only (`[submitter]
  ///        jobWaitTimeout`, a plain integer count of seconds); there is no
  ///        environment override, following the same convention as the other
  ///        tuning fields.
  std::chrono::seconds jobWaitTimeout{0};

  BackendRegistryConfig backendRegistry; ///< This submitter's registry timing.

  /// @brief Minimum spacing between two of this stage's own backend-status
  ///        publications.
  std::chrono::milliseconds backendStatusPublishInterval{5000};
};

/// @brief A daemon's complete configuration, grouped by the pipeline
///        responsibility each field belongs to. A shipped entrypoint reads
///        whichever groups its own work touches, not only the group named
///        after it:
///        - The distributed selector reads `[common]` and `[selector]`.
///        - The distributed worker reads `[common]`,
///          `[selector].backendStatusQueue`, `[compiler]`, and
///          `[submitter]`.
///        - The standalone daemon reads `[common]`,
///          `[selector].selectionPolicy`, and `[submitter]`.
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
/// defaults/environment values. A file that exists but fails to parse, an
/// unknown key, a present value of the wrong type, or a value that violates
/// one of the range/cross-field constraints documented on the affected
/// field is reported as `Error::Kind::ConfigError` naming the exact dotted
/// key at fault.
/// @return The assembled configuration, or a ConfigError describing why the
///         discovered file could not be read or the assembled configuration
///         is invalid.
std::expected<Config, Error> loadConfig();

/// @brief Build a configuration from defaults, the TOML file at @p
///        configFile, and environment overrides, bypassing the usual
///        `QRMCI_CONFIG_FILE`/default-path discovery.
/// @param configFile The TOML file to load. A missing file is not an error.
/// @return The assembled configuration, or a ConfigError describing why @p
///         configFile could not be read or the assembled configuration is
///         invalid.
std::expected<Config, Error>
loadConfig(const std::filesystem::path &configFile);

} // namespace mqss::qrmci
