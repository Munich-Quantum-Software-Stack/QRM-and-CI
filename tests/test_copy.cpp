#include "../include/connection_handling.hpp"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct QuantumResult {
  int task_id;
  std::map<std::string, int> results;
  std::string destination;
  bool execution_status;
  std::vector<std::string> executed_qpu;
  std::vector<std::string> executed_circuit;
  std::string additional_information;
  double execution_time;
};

QuantumResult JSONToQuantumResult(const char *QuantumResult_str) {
  QuantumResult result;

  json QuantumResult_json = json::parse(QuantumResult_str);

  result.task_id = QuantumResult_json["task_id"];
  result.results = QuantumResult_json["results"];
  result.destination = QuantumResult_json["destination"];
  result.execution_status = QuantumResult_json["execution_status"];
  result.executed_qpu =
      QuantumResult_json["executed_qpu"].get<std::vector<std::string>>();
  result.executed_circuit =
      QuantumResult_json["executed_circuit"].get<std::vector<std::string>>();
  result.additional_information = QuantumResult_json["additional_information"];
  result.execution_time = QuantumResult_json["execution_time"];

  return result;
}

std::string *run_test(int argc, char *argv[]) {
  setbuf(stdout, NULL);

  // Open the QIR file
  const char *filename = "/home/ubuntu/qrm.git/benchmarks/test.ll";
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "[Quantum Daemon]......Failed to open file with QIR: "
              << filename << std::endl;
    std::string message = "Failed to open file with QIR: ";
    return new std::string(message);
  }

  // Read the file with the generic QIR
  const std::streamsize chunkSize = 1024;
  char bufferQir[chunkSize];
  std::string genericQir;

  while (!file.eof()) {
    file.read(bufferQir, chunkSize);
    genericQir.append(bufferQir, file.gcount());
  }
  file.close();

  // Create JSON string to send to the Quantum Resource Manager
  std::time_t currentTime = std::time(nullptr);
  const int bufferSize = 80;
  char buffer[bufferSize];
  std::strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S",
                std::localtime(&currentTime));
  std::string submit_time(buffer);
  json QuantumTask_json = {
      {"task_id", -1},
      {"n_qbits", 0},
      {"n_shots", 10000},
      {"circuit_file", ""},
      {"circuit_file_type", "QIR"},
      {"result_destination", ""},
      {"preferred_qpu", "Q20"},
      {"scheduled_qpu", ""},
      {"priority", 0},
      {"optimisation_level", 0},
      {"no_modify", false},
      {"transpiler_flag", true},
      {"result_type", 0},
      {"submit_time", submit_time},
      {"circuit_qiskit", genericQir},
      {"additional_information", ""},
      {"change_selector", "libselector_manual.so"},
      {"change_scheduler", "libscheduler_heuristic.so"},
  };

  std::string QuantumTask_str = QuantumTask_json.dump();

  // Send the QuantumTask to the Quantum Resource Manager
  std::cout << "[Quantum Daemon]......Sending QuantumTask to the QRM"
            << std::endl;

  return new std::string(QuantumTask_str);
}
