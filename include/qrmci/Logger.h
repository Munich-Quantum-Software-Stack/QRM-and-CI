/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file Logger.h
/// @brief The console-plus-file logger every QRM&CI daemon installs as its
///        process-wide default.

#pragma once

#include <cstddef>
#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

namespace mqss::qrmci {

/// @brief Largest a single log file grows before it is rotated.
inline constexpr std::size_t MaxLogFileBytes = 50UL * 1024 * 1024;
/// @brief How many rotated log files are kept alongside the current one.
inline constexpr std::size_t MaxLogFiles = 5;

/// @brief Create a logger writing to the console and, if @p logFile is given,
///        to a rotating file alongside it.
/// @param name The logger name, as it appears in each log line.
/// @param logFile The file to rotate through, or empty for console only.
/// @return The logger, ready to be installed as the process-wide default.
inline std::shared_ptr<spdlog::logger>
makeLogger(std::string_view name, std::string_view logFile = {}) {
  std::vector<spdlog::sink_ptr> sinks{
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
  };

  if (!logFile.empty()) {
    // A log directory that cannot be written is a deployment problem worth
    // reporting, but it is not a reason to refuse to run: the console sink
    // still carries every line. Letting the exception out would abort the
    // process before it could say why.
    try {
      sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          std::string(logFile), MaxLogFileBytes, MaxLogFiles));
    } catch (const spdlog::spdlog_ex &e) {
      spdlog::warn("Logging to console only; could not open '{}': {}", logFile,
                   e.what());
    }
  }

  auto logger = std::make_shared<spdlog::logger>(std::string(name),
                                                 sinks.begin(), sinks.end());

  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
  logger->set_level(spdlog::level::info);
  logger->flush_on(spdlog::level::info);

  return logger;
}

} // namespace mqss::qrmci
