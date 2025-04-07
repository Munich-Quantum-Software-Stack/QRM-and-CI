#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace mqss {
struct QuantumTask {
  boost::uuids::uuid task_id;
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

QuantumTask dumpJsonToQuantumTask(const char *QuantumTaskAsString);
json dumpQuantumTaskToJson(const QuantumTask &task);
void dumpQuantumTask(const QuantumTask &quantumTask);
} // namespace mqss
