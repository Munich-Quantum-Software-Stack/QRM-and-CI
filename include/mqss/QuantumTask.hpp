#pragma once

#include <string>

namespace mqss {
struct QuantumTask {
  int task_id;
  int parent_id;
  int n_qbits;
  int n_shots;
  std::string circuit_file;
  std::string circuit_file_type;
  std::string result_destination;
  std::string preferred_qpu;
  // QDMI_Device scheduled_qpu;
  int priority;
  int optimisation_level;
  bool no_modify;
  bool transpiler_flag;
  int result_type;
  std::string submit_time;
  std::string quake;
  std::string additional_information;
  // ThreadSafeModule thread_safe_module;
};

} // namespace mqss
