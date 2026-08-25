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
/// process-wide runtime configuration from defaults and environment overrides.

#pragma once

#include <string>

namespace mqss::qrmci {

/// @brief RabbitMQ connection settings.
struct RabbitMqConfig {
  std::string host;     ///< RabbitMQ host name.
  int port;             ///< RabbitMQ TCP port.
  std::string user;     ///< RabbitMQ user name.
  std::string password; ///< RabbitMQ password.
  std::string vhost;    ///< RabbitMQ virtual host.
};

/// @brief Queue names used to connect the QRM pipeline stages.
struct QueueConfig {
  std::string qrmci;     ///< Input queue for the QRM daemon.
  std::string scheduler; ///< Queue used by the scheduler stage.
  std::string compiler;  ///< Queue used by the compiler stage.
  std::string results;   ///< Queue that carries execution results.
  std::string submitter; ///< Queue used by the submitter stage.
};

/// @brief Logger names and log file locations for each component.
struct LoggingConfig {
  std::string logDir; ///< Directory where log files are written.

  std::string daemonLogger;    ///< Logger name for the daemon.
  std::string schedulerLogger; ///< Logger name for the scheduler.
  std::string compilerLogger;  ///< Logger name for the compiler.
  std::string submitterLogger; ///< Logger name for the submitter.

  std::string daemonLog;    ///< Log file for the daemon.
  std::string schedulerLog; ///< Log file for the scheduler.
  std::string compilerLog;  ///< Log file for the compiler.
  std::string submitterLog; ///< Log file for the submitter.
};

/// @brief Submitter-specific QDMI credentials and device selection.
struct SubmitterConfig {
  std::string qdmiDriverName;  ///< QDMI driver name.
  std::string qdmiDeviceName;  ///< QDMI device name.
  std::string qdmiClientToken; ///< QDMI client token.
};

/// @brief Aggregate configuration for the QRM daemon.
struct Config {
  RabbitMqConfig rabbitmq;   ///< RabbitMQ connection settings.
  QueueConfig queues;        ///< Queue configuration.
  LoggingConfig logging;     ///< Logging configuration.
  SubmitterConfig submitter; ///< Submitter configuration.
};

/// @brief Build a configuration from defaults and environment overrides.
Config loadConfig(int argc, char **argv);

/// @brief Store the process-wide configuration.
void initConfig(Config config);

/// @brief Return the initialized process-wide configuration.
const Config &getConfig();

} // namespace mqss::qrmci
