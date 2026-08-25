/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

// MQSS compiler to provide a consistent interface for the
// QRM workflow. This is responsible for invoking the MQSS
// compiler with the appropriate arguments and handling the output. This will be
// deprecated once the MQSS compiler provides a consistent interface for the QRM
// workflow example.

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mqss {

struct CompilerOptions {
  int optimizationLevel;
  std::string resultType;
};

class MQSSCompiler {
public:
  // Constructor for the MQSS Compiler.
  // @param mqssOptPath The path to the MQSS compiler executable.
  explicit MQSSCompiler(std::string mqssOptPath = "mqss-opt")
      : mqssOpt(std::move(mqssOptPath)) {
    checkExecutable(mqssOpt);
  }

  // Invokes the MQSS compiler with the provided arguments.
  // @param input_circuit The input circuit to compile.
  // @param backend_name The name of the target backend.
  // @param native_gates The list of native gates for the target quantum
  // hardware.
  // @param qubit_connectivity The qubit connectivity map for the target quantum
  // hardware.
  // @param opts The compiler options.
  // @return The compiled circuit as a string.
  std::string compile(const std::string &inputCircuit,
                      const std::string &backendName,
                      const std::vector<std::string> &nativeGates,
                      const std::vector<std::pair<std::uint32_t, std::uint32_t>>
                          &qubitConnectivity,
                      const CompilerOptions &opts);

  // Overloaded compile method for cases where only the backend name is
  // provided.
  // @param input_circuit The input circuit to compile.
  // @param backend_name The name of the target backend.
  // @param opts The compiler options.
  // @return The compiled circuit as a string.
  std::string compile(const std::string &inputCircuit,
                      const std::string &backendName,
                      const CompilerOptions &opts) {
    return compile(inputCircuit, backendName, {}, {}, opts);
  }

  // Overloaded compile method for cases where only the input circuit and native
  // gates are provided.
  // @param input_circuit The input circuit to compile.
  // @param native_gates The list of native gates for the target quantum
  // hardware.
  // @param opts The compiler options.
  // @return The compiled circuit as a string.
  std::string compile(const std::string &inputCircuit,
                      const std::vector<std::string> &nativeGates,
                      const CompilerOptions &opts) {
    return compile(inputCircuit, "", nativeGates, {}, opts);
  }

  // Overloaded compile method for cases where only the input circuit and native
  // gates are provided.
  // @param input_circuit The input circuit to compile.
  // @param native_gates The list of native gates for the target quantum
  // hardware.
  // @param opts The compiler options.
  // @return The compiled circuit as a string.
  std::string compile(const std::string &inputCircuit,
                      const std::vector<std::string> &nativeGates,
                      const std::vector<std::pair<std::uint32_t, std::uint32_t>>
                          &qubitConnectivity,
                      const CompilerOptions &opts) {
    return compile(inputCircuit, "", nativeGates, qubitConnectivity, opts);
  }

  // Overloaded compile method for cases where only the input circuit is
  // provided.
  // @param input_circuit The input circuit to compile.
  // @param opts The compiler options.
  // @return The compiled circuit as a string.
  std::string compile(const std::string &inputCircuit,
                      const CompilerOptions &opts) {
    return compile(inputCircuit, "", {}, {}, opts);
  }

private:
  std::string mqssOpt;

  static std::string stripSpuriousGateDefs(const std::string &qasm);

  static void checkExecutable(const std::string &executable) {
    if (std::system((executable + " --version > /dev/null 2>&1").c_str()) !=
        0) {
      throw std::runtime_error(executable +
                               " executable not found in system path.");
    }
  }
};

} // namespace mqss
