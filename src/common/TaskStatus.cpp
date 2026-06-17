#include "common/TaskStatus.hpp"

// Function to convert enum to string (for easy printing)
const char *mqss::to_string(mqss::TaskStatus status) {
  switch (status) {
  case mqss::TaskStatus::RUNNING:
    return "RUNNING";
  case mqss::TaskStatus::CANCELLED:
    return "CANCELLED";
  case mqss::TaskStatus::COMPLETED:
    return "COMPLETED";
  case mqss::TaskStatus::UNKNOWN:
    return "UNKNOWN";
  default:
    return "UNKNOWN";
  }
}

nlohmann::json mqss::getErrorAnswer(int status,
                                    const std::string &error_message) {
  nlohmann::json jsonResponse = {{"status", std::to_string(status)},
                                 {"error", error_message.c_str()}};
  return jsonResponse;
}
