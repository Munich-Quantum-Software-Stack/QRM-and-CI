

#include "mqss/Utils/Logger.hpp"

#include <iostream>

std::shared_ptr<spdlog::logger> Logger::logger;

void Logger::init() {
  // Initialize the logger with a console sink
  logger = spdlog::stdout_color_mt("console");
  logger->set_level(spdlog::level::info); // Set default logging level
  logger->set_pattern("%^[QRM::%Y-%m-%d %H:%M:%S] [%l] %v%$");

  // logger->set_pattern("%^[QRM::%Y-%m-%d %H:%M:%S] [%l]%$ %^[%v]%$");  //
  // Custom pattern
}

std::shared_ptr<spdlog::logger> Logger::getLogger() { return logger; }
