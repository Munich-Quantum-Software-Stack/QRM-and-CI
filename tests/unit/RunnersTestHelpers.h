/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file RunnersTestHelpers.h
/// @brief Shared BackendWrapper/QuantumTask fixture factories used by the
///        Runners.cpp test suites (TestRunnersBackendSelection,
///        TestRunnersTaskCompilation, TestRunnersTaskExecution).

#pragma once

#include "BackendWrapperTestBuilder.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mqss::qrmci::test {

inline mqss::qrmci::BackendWrapper makeBackend(
    const std::string &name, std::uint32_t numQubits = 5,
    mqss::BackendStatus status = mqss::BackendStatus::BACKEND_STATUS_IDLE,
    const std::vector<mqss::CircuitFormat> &formats = {
        mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3}) {
  return BackendBuilder()
      .name(name)
      .numQubits(numQubits)
      .status(status)
      .instructions({"rx", "cz", "measure"})
      .supportedCircuitFormats(formats)
      .build();
}

inline mqss::QuantumTask makeTask(const std::string &circuit = "OPENQASM 3.0;",
                                  std::uint32_t numQubits = 2,
                                  std::string type = "qasm3") {
  mqss::QuantumTask task;
  task.add_circuit_files({circuit});
  task.set_n_qbits(static_cast<int>(numQubits));
  task.set_n_shots(10);
  task.set_circuit_file_type(type);
  return task;
}

} // namespace mqss::qrmci::test
