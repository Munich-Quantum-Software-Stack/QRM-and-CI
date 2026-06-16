#include "common/Logger.hpp"

namespace mqss {
// Initialize the static logger instance
std::shared_ptr<spdlog::logger> Logger::logger = nullptr;
std::once_flag Logger::init_flag;
std::string Logger::loggerName;

void Logger::init(const std::string &logFilePath, const std::string &name) {
  std::call_once(init_flag, [&]() {
    // Set the logger name
    loggerName = name.empty() ? "default_logger" : name;

    // Create a vector of sinks (file sink and console sink)
    auto file_sink =
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // Combine the sinks into a multi-sink logger
    std::vector<spdlog::sink_ptr> sinks{file_sink, console_sink};
    logger = std::make_shared<spdlog::logger>(loggerName, sinks.begin(),
                                              sinks.end());

    // Set the logging pattern
    logger->set_pattern("%^[" + name + "::%Y-%m-%d %H:%M:%S] [%l] %v%$");

    // Set the default logging level
    logger->set_level(spdlog::level::info);
  });
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
