/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "MQSSCompiler.hpp"

#include <fstream>
#include <sstream>

namespace mqss {

std::string
MQSSCompiler::compile(const std::string &input_circuit,
                      const std::string &backend_name,
                      const std::vector<std::string> &native_gates,
                      const std::unordered_map<int, int> &qubit_connectivity,
                      const CompilerOptions &opts) {

  checkExecutable(mqss_opt);

  std::string input_file = "/tmp/input_circuit.mlir";
  std::string output_file = "/tmp/output_circuit.mlir";
  std::ofstream ofs(input_file);
  if (!ofs) {
    throw std::runtime_error("Failed to create temporary input file.");
  }
  ofs << input_circuit;
  ofs.close();

  std::string basis_gates = "phased_rx,cz";

  std::string shell_command =
      mqss_opt + " " + input_file + " --O" +
      std::to_string(opts.optimization_level) +
      " --cse --canonicalize --BasisConversionPass=gates=" + basis_gates +
      " > " + output_file;

  int ret = std::system(shell_command.c_str());
  if (ret != 0) {
    std::remove(output_file.c_str());
    throw std::runtime_error("MQSS compiler pipeline failed with code: " +
                             std::to_string(ret));
  }

  std::string output_circuit;
  char buffer[4096];
  std::ifstream ifs(output_file);
  if (!ifs) {
    throw std::runtime_error("Failed to read output circuit file.");
  }
  while (ifs.getline(buffer, sizeof(buffer))) {
    output_circuit += buffer;
  }
  ifs.close();
  std::remove(output_file.c_str());

  std::remove(input_file.c_str());

  if (opts.result_type == "openqasm2") {
    output_circuit = stripSpuriousGateDefs(output_circuit);
  }

  return output_circuit;
}

std::string MQSSCompiler::stripSpuriousGateDefs(const std::string &qasm) {
  std::istringstream stream(qasm);
  std::ostringstream result;
  std::string line;
  bool inGateBlock = false;
  while (std::getline(stream, line)) {
    if (line.find("gate ") != std::string::npos &&
        line.find("{") != std::string::npos) {
      inGateBlock = true;
      continue;
    }
    if (inGateBlock) {
      if (line.find("}") != std::string::npos) {
        inGateBlock = false;
      }
      continue;
    }
    result << line << "\n";
  }
  return result.str();
}

} // namespace mqss
