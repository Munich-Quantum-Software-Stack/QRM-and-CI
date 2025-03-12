#pragma once

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

class Logger {
public:
  static void init();                                 // Initialize logger
  static std::shared_ptr<spdlog::logger> getLogger(); // Get logger instance

private:
  static std::shared_ptr<spdlog::logger> logger;
};
