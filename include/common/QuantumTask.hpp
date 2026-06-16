/* This code and any associated documentation is provided "as is"

Copyright 2024 Munich Quantum Software Stack Project

Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
"License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

TODO

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-------------------------------------------------------------------------
  author Martin Letras
  date   April 2025
  version 1.0
  brief
        Struct defining the quantum task. Also defining the signature functions
        to manipulate a quantum task.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#pragma once

#include "common/RuntimeMLIR.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Parser/Parser.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace mqss {
struct QuantumTask {
  std::string task_id;
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
std::vector<mlir::ModuleOp> getMLIRModules(const QuantumTask &quantumTask);
std::tuple<mlir::ModuleOp, mlir::MLIRContext *>
extractMLIRContext(const std::string &quakeModule);
} // namespace mqss
