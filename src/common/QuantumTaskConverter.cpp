#include "mqss/common/QuantumTask.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

mqss::QuantumTask mqss::JSONToQuantumTask(const char *quantumTaskAsString) {
  QuantumTask quantumTask;
  json jsonQuantumTask = json::parse(quantumTaskAsString);

  if (!jsonQuantumTask.contains("task_id")) {
    std::cerr << "Field task_id was not defined in json file" << std::endl;
    return QuantumTask();
  }
  jsonQuantumTask.at("task_id").get_to(quantumTask.task_id);
  jsonQuantumTask.at("n_qbits").get_to(quantumTask.n_qbits);
  jsonQuantumTask.at("n_shots").get_to(quantumTask.n_shots);
  if (!jsonQuantumTask.contains("circuit_files")) {
    std::cerr << "Field circuit_files was not defined in json file"
              << std::endl;
    return QuantumTask();
  }
  jsonQuantumTask.at("circuit_files").get_to(quantumTask.circuit_files);
  jsonQuantumTask.at("circuit_file_type").get_to(quantumTask.circuit_file_type);
  jsonQuantumTask.at("result_destination")
      .get_to(quantumTask.result_destination);
  jsonQuantumTask.at("preferred_qpu").get_to(quantumTask.preferred_qpu);
  jsonQuantumTask.at("scheduled_qpu").get_to(quantumTask.scheduled_qpu);
  jsonQuantumTask.at("priority").get_to(quantumTask.priority);
  jsonQuantumTask.at("optimisation_level")
      .get_to(quantumTask.optimisation_level);
  jsonQuantumTask.at("no_modify").get_to(quantumTask.no_modify);
  jsonQuantumTask.at("transpiler_flag").get_to(quantumTask.transpiler_flag);
  jsonQuantumTask.at("result_type").get_to(quantumTask.result_type);
  jsonQuantumTask.at("submit_time").get_to(quantumTask.submit_time);
  jsonQuantumTask.at("circuits_qiskit").get_to(quantumTask.circuits_qiskit);
  jsonQuantumTask.at("additional_information")
      .get_to(quantumTask.additional_information);
  jsonQuantumTask.at("restricted_resource_names")
      .get_to(quantumTask.restricted_resource_names);
  jsonQuantumTask.at("user_identity").get_to(quantumTask.user_identity);
  jsonQuantumTask.at("token").get_to(quantumTask.token);
  jsonQuantumTask.at("via_hpc").get_to(quantumTask.via_hpc);
  return quantumTask;
}
