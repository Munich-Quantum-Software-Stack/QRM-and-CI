/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/BackendWrapper.h"

#include "Submitter.h"
#include "mqss/Protocol.hpp"
#include "qdmi/constants.h"

#include <algorithm>
#include <memory>

namespace {
[[nodiscard]] mqss::BackendStatus
mapBackendStatus(QDMI_Device_Status status) noexcept;
[[nodiscard]] mqss::CircuitFormat
mapCircuitFormat(QDMI_Program_Format format) noexcept;
} // namespace

mqss::qrmci::BackendWrapper::BackendWrapper(
    mqss::submitter::Submitter &submitter)
    : name(submitter.getDeviceID()),
      numQubits(static_cast<std::uint32_t>(submitter.getDeviceNumQubits())),
      type(mqss::BackendType::BACKEND_TYPE_UNSPECIFIED),
      status(mapBackendStatus(submitter.getDeviceStatus())), queueLength(0),
      currentLoad(0.0f), queueName("") {

  instructions = submitter.getDeviceInstructions();
  std::vector<std::pair<size_t, size_t>> connectivityPairs =
      submitter.getDeviceConnectivity();
  qubitConnectivity.reserve(connectivityPairs.size());
  for (const auto &pair : connectivityPairs) {
    qubitConnectivity.emplace_back(pair.first, pair.second);
  }
  std::vector<QDMI_Program_Format> supportedFormats =
      submitter.getDeviceSupportedCircuitFormats();
  supportedCircuitFormats.reserve(supportedFormats.size());
  std::ranges::transform(
      supportedFormats, std::back_inserter(supportedCircuitFormats),
      [](QDMI_Program_Format format) { return mapCircuitFormat(format); });
}

mqss::qrmci::BackendWrapper::BackendWrapper(const mqss::Backend &backend)
    : name(backend.name()), numQubits(backend.num_qubits()),
      type(backend.type()), status(backend.status()),
      queueLength(backend.queue_length()), currentLoad(backend.current_load()),
      queueName(backend.queue_name()) {

  instructions.assign(backend.instructions().begin(),
                      backend.instructions().end());
  qubitConnectivity.reserve(static_cast<size_t>(backend.connectivity().size()));
  for (const mqss::QubitPair &qpair : backend.connectivity()) {
    qubitConnectivity.emplace_back(qpair.qubit1(), qpair.qubit2());
  }
  supportedCircuitFormats.reserve(
      static_cast<size_t>(backend.supported_circuit_formats().size()));
  for (const auto &format : backend.supported_circuit_formats()) {
    supportedCircuitFormats.push_back(static_cast<mqss::CircuitFormat>(format));
  }
}

[[nodiscard]] mqss::Backend mqss::qrmci::BackendWrapper::makeBackend() {
  mqss::Backend backend;
  backend.set_name(name);
  backend.set_num_qubits(numQubits);
  backend.set_type(type);
  backend.set_status(status);
  backend.set_queue_length(queueLength);
  backend.set_current_load(currentLoad);
  backend.set_queue_name(queueName);

  for (const auto &instr : instructions) {
    backend.add_instructions(instr);
  }
  for (const auto &pair : qubitConnectivity) {
    mqss::QubitPair *qpair = backend.add_connectivity();
    qpair->set_qubit1(pair.first);
    qpair->set_qubit2(pair.second);
  }
  for (const auto &format : supportedCircuitFormats) {
    backend.add_supported_circuit_formats(format);
  }

  return backend;
}

namespace {
[[nodiscard]] mqss::BackendStatus
mapBackendStatus(QDMI_Device_Status status) noexcept {
  switch (status) {
  case QDMI_DEVICE_STATUS_OFFLINE:
    return mqss::BackendStatus::BACKEND_STATUS_OFFLINE;
  case QDMI_DEVICE_STATUS_IDLE:
    return mqss::BackendStatus::BACKEND_STATUS_IDLE;
  case QDMI_DEVICE_STATUS_BUSY:
    return mqss::BackendStatus::BACKEND_STATUS_BUSY;
  case QDMI_DEVICE_STATUS_ERROR:
    return mqss::BackendStatus::BACKEND_STATUS_ERROR;
  case QDMI_DEVICE_STATUS_MAINTENANCE:
    return mqss::BackendStatus::BACKEND_STATUS_MAINTENANCE;
  case QDMI_DEVICE_STATUS_CALIBRATION:
    return mqss::BackendStatus::BACKEND_STATUS_CALIBRATION;
  default:
    return mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED;
  }
}

[[nodiscard]] mqss::CircuitFormat
mapCircuitFormat(QDMI_Program_Format format) noexcept {
  switch (format) {
  case QDMI_PROGRAM_FORMAT_QASM2:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2;
  case QDMI_PROGRAM_FORMAT_QASM3:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3;
  case QDMI_PROGRAM_FORMAT_QIRBASESTRING:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING;
  case QDMI_PROGRAM_FORMAT_QIRBASEMODULE:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE;
  case QDMI_PROGRAM_FORMAT_QIRADAPTIVESTRING:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING;
  case QDMI_PROGRAM_FORMAT_QIRADAPTIVEMODULE:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE;
  case QDMI_PROGRAM_FORMAT_CALIBRATION:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_CALIBRATION;
  case QDMI_PROGRAM_FORMAT_QPY:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QPY;
  case QDMI_PROGRAM_FORMAT_IQMJSON:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_IQMJSON;
  case QDMI_PROGRAM_FORMAT_BATCHJOB:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_BATCHJOB;
  default:
    return mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED;
  }
}
} // namespace
