/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "MQSSCompiler.h"

#include <array>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace mqss {

std::string
MQSSCompiler::compile(const std::string &inputCircuit,
                      const std::string & /*backendName*/,
                      const std::vector<std::string> & /*nativeGates*/,
                      const std::vector<std::pair<std::uint32_t, std::uint32_t>>
                          & /*qubitConnectivity*/,
                      const CompilerOptions &opts) {

  checkExecutable(mqssOpt);

  std::string inputFile = "/tmp/input_circuit.mlir";
  std::string outputFile = "/tmp/output_circuit.mlir";
  std::ofstream ofs(inputFile);
  if (!ofs) {
    throw std::runtime_error("Failed to create temporary input file.");
  }
  ofs << inputCircuit;
  ofs.close();

  std::string basisGates = "phased_rx,cz";

  std::string shellCommand =
      mqssOpt + " " + inputFile + " --O" +
      std::to_string(opts.optimizationLevel) +
      " --cse --canonicalize --BasisConversionPass=gates=" + basisGates +
      " > " + outputFile;

  int ret = std::system(shellCommand.c_str());
  if (ret != 0) {
    std::remove(outputFile.c_str());
    throw std::runtime_error("MQSS compiler pipeline failed with code: " +
                             std::to_string(ret));
  }

  std::string outputCircuit;
  std::array<char, 4096> buffer{};
  std::ifstream ifs(outputFile);
  if (!ifs) {
    throw std::runtime_error("Failed to read output circuit file.");
  }
  while (
      ifs.getline(buffer.data(), static_cast<std::streamsize>(buffer.size()))) {
    outputCircuit += buffer.data();
  }
  ifs.close();
  std::remove(outputFile.c_str());

  std::remove(inputFile.c_str());

  if (opts.resultType == "openqasm2") {
    outputCircuit = stripSpuriousGateDefs(outputCircuit);
  }

  return outputCircuit;
}

std::string MQSSCompiler::stripSpuriousGateDefs(const std::string &qasm) {
  std::istringstream stream(qasm);
  std::ostringstream result;
  std::string line;
  bool inGateBlock = false;
  while (std::getline(stream, line)) {
    if (line.contains("gate ") && line.contains("{")) {
      inGateBlock = true;
      continue;
    }
    if (inGateBlock) {
      if (line.contains("}")) {
        inGateBlock = false;
      }
      continue;
    }
    result << line << "\n";
  }
  return result.str();
}

} // namespace mqss
