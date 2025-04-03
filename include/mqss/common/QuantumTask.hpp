#pragma once

#include <string>
#include <vector>

namespace mqss {
struct QuantumTask {
  int task_id;
  int n_qbits;
  int n_shots;
  std::vector<std::string> circuit_files;
  std::string circuit_file_type; // qasm, quake or qir
  std::string result_destination;
  std::string preferred_qpu;
  std::string scheduled_qpu;
  // QDMI_Device scheduled_qpu;
  int priority;
  int optimisation_level;
  bool no_modify;
  bool transpiler_flag;
  int result_type;
  std::string submit_time;
  std::vector<std::string> circuits_qiskit; // TODO
  std::string additional_information;
  std::vector<std::string> restricted_resource_names;
  std::string user_identity;
  std::string token;
  bool via_hpc;
  // ThreadSafeModule thread_safe_module;
};

QuantumTask JSONToQuantumTask(const char *QuantumTaskAsString);

} // namespace mqss
