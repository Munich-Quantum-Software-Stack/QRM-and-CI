/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file Config.hpp
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
  std::string log_dir; ///< Directory where log files are written.

  std::string daemon_logger;    ///< Logger name for the daemon.
  std::string scheduler_logger; ///< Logger name for the scheduler.
  std::string compiler_logger;  ///< Logger name for the compiler.
  std::string submitter_logger; ///< Logger name for the submitter.

  std::string daemon_log;    ///< Log file for the daemon.
  std::string scheduler_log; ///< Log file for the scheduler.
  std::string compiler_log;  ///< Log file for the compiler.
  std::string submitter_log; ///< Log file for the submitter.
};

/// @brief Filesystem paths used by the runtime.
struct PathConfig {
  std::string benchmark_dir; ///< Directory containing benchmark artifacts.
  std::string
      qdmi_device_objs_dir; ///< Directory containing QDMI device objects.
};

/// @brief QDMI device object metadata.
struct QDMIDevices {
  std::string qdmi_device_obj;    ///< QDMI device object name.
  std::string qdmi_device_prefix; ///< Prefix used for QDMI device names.
};

/// @brief Submitter-specific QDMI credentials and device selection.
struct SubmitterConfig {
  std::string qdmi_driver_name;  ///< QDMI driver name.
  std::string qdmi_device_name;  ///< QDMI device name.
  std::string qdmi_client_token; ///< QDMI client token.
};

/// @brief Aggregate configuration for the QRM daemon.
struct Config {
  RabbitMqConfig rabbitmq;   ///< RabbitMQ connection settings.
  QueueConfig queues;        ///< Queue configuration.
  LoggingConfig logging;     ///< Logging configuration.
  PathConfig paths;          ///< Filesystem paths.
  QDMIDevices devices;       ///< QDMI device metadata.
  SubmitterConfig submitter; ///< Submitter configuration.
};

/// @brief Build a configuration from defaults and environment overrides.
Config loadConfig(int argc, char **argv);

/// @brief Store the process-wide configuration.
void initConfig(Config config);

/// @brief Return the initialized process-wide configuration.
const Config &getConfig();

} // namespace mqss::qrmci
