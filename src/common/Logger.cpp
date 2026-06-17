#include "common/Logger.hpp"
#include <iostream>

namespace mqss {
// Initialize the static logger instance
std::shared_ptr<spdlog::logger> Logger::logger = nullptr;
std::once_flag Logger::init_flag;
std::string Logger::loggerName;

void Logger::init(const std::string &logFilePath, const std::string &name) {
    loggerName = name.empty() ? "default_logger" : name;

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks{file_sink, console_sink};
    logger = std::make_shared<spdlog::logger>(loggerName, sinks.begin(), sinks.end());
    logger->set_pattern("%^[" + name + "::%Y-%m-%d %H:%M:%S] [%l] %v%$");
    logger->set_level(spdlog::level::info);

    // Flush immediately on every info message and above
    logger->flush_on(spdlog::level::info);
}

std::shared_ptr<spdlog::logger> Logger::getLogger() {
  if (!logger) {
    throw std::runtime_error(
        "Logger not initialized. Call Logger::init() first.");
  }
  return logger;
}

void Logger::cleanup() {
  if (logger) {
    logger->flush();          // Flush any pending log messages
    spdlog::drop(loggerName); // Remove the logger from the registry using the
                              // unique name
    logger = nullptr;         // Reset the logger pointer
  }
}
} // namespace mqss
