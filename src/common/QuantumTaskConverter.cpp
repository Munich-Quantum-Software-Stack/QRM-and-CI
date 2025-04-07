#include "mqss/common/QuantumTask.hpp"

#include <iostream>

void mqss::dumpQuantumTask(const QuantumTask &quantumTask) {
  std::cout << "task_id " << quantumTask.task_id << std::endl;
  std::cout << "n_qbits " << quantumTask.n_qbits << std::endl;
  std::cout << "n_shots " << quantumTask.n_shots << std::endl;
  std::cout << "circuit_files: " << std::endl;
  for (const auto &cFile : quantumTask.circuit_files)
    std::cout << "\tcircuit file: " << cFile << std::endl;
  std::cout << "circuit_file_type " << quantumTask.circuit_file_type
            << std::endl;
  std::cout << "result_destination " << quantumTask.result_destination
            << std::endl;
  std::cout << "preferred_qpu " << quantumTask.preferred_qpu << std::endl;
  std::cout << "scheduled_qpu " << quantumTask.scheduled_qpu << std::endl;
  std::cout << "priority " << quantumTask.priority << std::endl;
  std::cout << "optimisation_level " << quantumTask.optimisation_level
            << std::endl;
  std::cout << "no_modify " << quantumTask.no_modify << std::endl;
  std::cout << "transpiler_flag " << quantumTask.transpiler_flag << std::endl;
  std::cout << "result_type " << quantumTask.result_type << std::endl;
  std::cout << "submit_time " << quantumTask.submit_time << std::endl;
  std::cout << "circuits_qiskit " << std::endl;
  for (const auto &cQiskit : quantumTask.circuits_qiskit)
    std::cout << "\tcircuit qiskit: " << cQiskit << std::endl;
  std::cout << "additional_information " << quantumTask.additional_information
            << std::endl;
  std::cout << "restricted_resource_names " << std::endl;
  for (const auto &rResource : quantumTask.restricted_resource_names)
    std::cout << "\trestricted_resource_name" << rResource << std::endl;
  std::cout << "user_identity " << quantumTask.user_identity << std::endl;
  std::cout << "token " << quantumTask.token << std::endl;
  std::cout << "via_hpc " << quantumTask.via_hpc << std::endl;
}

json mqss::dumpQuantumTaskToJson(const QuantumTask &task) {
  return {{"task_id", boost::uuids::to_string(task.task_id)},
          {"n_qbits", task.n_qbits},
          {"n_shots", task.n_shots},
          {"circuit_files", task.circuit_files},
          {"circuit_file_type", task.circuit_file_type},
          {"result_destination", task.result_destination},
          {"preferred_qpu", task.preferred_qpu},
          {"scheduled_qpu", task.scheduled_qpu},
          {"priority", task.priority},
          {"optimisation_level", task.optimisation_level},
          {"no_modify", task.no_modify},
          {"transpiler_flag", task.transpiler_flag},
          {"result_type", task.result_type},
          {"submit_time", task.submit_time},
          {"circuits_qiskit", task.circuits_qiskit},
          {"additional_information", task.additional_information},
          {"restricted_resource_names", task.restricted_resource_names},
          {"user_identity", task.user_identity},
          {"token", task.token},
          {"via_hpc", task.via_hpc}};
}
mqss::QuantumTask mqss::dumpJsonToQuantumTask(const char *quantumTaskAsString) {
  QuantumTask quantumTask;
  json jsonQuantumTask = json::parse(quantumTaskAsString);

  if (!jsonQuantumTask.contains("task_id")) {
    std::cerr << "Field task_id was not defined in json file" << std::endl;
    return QuantumTask();
  }
  std::string uuid_from_json = jsonQuantumTask["task_id"];
  boost::uuids::string_generator gen;
  boost::uuids::uuid id = gen(uuid_from_json);
  quantumTask.task_id = id;
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
