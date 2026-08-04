/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "MQSSCompiler.hpp"
#include "qdmi/constants.h"
#include "qrmci.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <unordered_map>

void mqss::qrmci::select_backend(
    mqss::QuantumTask &task,
    const std::vector<mqss::qrmci::Backend> &available_backends) {
  // This function should implement the logic to select a backend for the
  // quantum task. For now, we will just select the first available backend that
  // matches the preferred QPU.
  for (const auto &backend : available_backends) {
    if (backend.name == task.preferred_qpu()) {
      task.set_scheduled_qpu(backend.name);
      return;
    }
  }

  // If no preferred backend is found, set the scheduled QPU to an empty string
  // or handle it as needed.
  task.set_scheduled_qpu("");
}

void mqss::qrmci::compile_quantum_task(mqss::QuantumTask &task) {
  std::unordered_map<int, std::string> updated_circuit_files;

  auto input_circuit_files = task.circuit_files();

  MQSSCompiler compiler;

  for (int i = 0; i < input_circuit_files.size(); ++i) {
    CompilerOptions opts;
    opts.optimization_level = task.optimisation_level();
    // result_type can be set to:
    // "qir", "qir-full", "qir-adaptive", "qir-base", "openqasm2"
    opts.result_type = "openqasm2";
    const auto &circuit_file = input_circuit_files[i];

    updated_circuit_files[i] =
        compiler.compile(circuit_file, task.preferred_qpu(), opts);
  }

  for (const auto &[index, new_circuit_file] : updated_circuit_files) {
    task.set_circuit_files(index, new_circuit_file);
  }
}

mqss::QuantumResult
mqss::qrmci::submit_quantum_task(const mqss::QuantumTask &task,
                                 mqss::Submitter &submitter) {
  auto results = submitter.submitTask(
      {task.circuit_files().begin(), task.circuit_files().end()},
      task.n_shots(), QDMI_PROGRAM_FORMAT_QIRBASESTRING);

  mqss::QuantumResult quantumResult;
  quantumResult.set_task_id(task.task_id());
  quantumResult.set_destination(task.result_destination());
  quantumResult.set_executed_qpu(task.scheduled_qpu());

  if (std::any_of(results.begin(), results.end(),
                  [](const std::optional<std::map<std::string, size_t>> &r) {
                    return r.has_value();
                  })) {
    quantumResult.set_execution_status(true);
    quantumResult.set_additional_information("COMPLETED");
    quantumResult.mutable_results()->Reserve(results.size());
    for (const auto &opt_map : results) {
      if (opt_map.has_value()) {
        auto *proto_counts = quantumResult.add_results()->mutable_counts();
        for (const auto &[key, val] : *opt_map) {
          (*proto_counts)[key] = static_cast<int32_t>(val);
        }
      }
    }

  } else {
    quantumResult.set_execution_status(false);
    quantumResult.set_additional_information("FAILED");
  }

  return quantumResult;
}
