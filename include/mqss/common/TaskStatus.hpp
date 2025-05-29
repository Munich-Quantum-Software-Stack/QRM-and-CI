#pragma once

#include <nlohmann/json.hpp>

namespace mqss {
// Enum definition for task states
enum class TaskStatus { RUNNING, CANCELLED, COMPLETED, UNKNOWN };

// Function to convert enum to string (for easy printing)
const char *to_string(TaskStatus status);

nlohmann::json getErrorAnswer(int status, const std::string &error_message);

} // namespace mqss
