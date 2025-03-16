#pragma once

#include <memory>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

namespace mqss {

class Logger {
public:
  // Initialize the logger (call this once in the main process)
  static void init(const std::string &logFilePath,
                   const std::string &loggerName = "default_logger");

  // Get the logger instance (can be called by any process or thread)
  static std::shared_ptr<spdlog::logger> getLogger();

  // Clean up the logger (call this before exiting)
  static void cleanup();

private:
  static std::shared_ptr<spdlog::logger> logger; // Shared logger instance
  static std::once_flag init_flag;               // Ensure single initialization
  static std::string loggerName;                 // Unique logger name
};

} // namespace mqss
