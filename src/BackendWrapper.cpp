/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/BackendWrapper.h"

#include "Submitter.h"
#include "mqss/Protocol.hpp"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

mqss::qrmci::BackendWrapper::BackendWrapper(
    mqss::submitter::Submitter &submitter)
    : name(submitter.getDeviceID()),
      numQubits(static_cast<std::uint32_t>(submitter.getDeviceNumQubits())),
      type(mqss::BackendType::BACKEND_TYPE_UNSPECIFIED),
      status(mapBackendStatus(submitter.getDeviceStatus())), queueLength(0),
      currentLoad(0.0F) {
  instructions = submitter.getDeviceInstructions();
  std::vector<std::pair<size_t, size_t>> const connectivityPairs =
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
    supportedCircuitFormats.push_back(mapProtoCircuitFormat(format));
  }
}

mqss::Backend mqss::qrmci::BackendWrapper::toBackend() const {
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

bool mqss::qrmci::BackendWrapper::isOnline() const noexcept {
  return isOnlineBackendStatus(status);
}

bool mqss::qrmci::BackendWrapper::canRun(const mqss::QuantumTask &task) const {
  if (task.n_qbits() < 0 ||
      static_cast<std::uint32_t>(task.n_qbits()) > numQubits) {
    return false;
  }
  return isCircuitTypeCompatibleWithFormats(task.circuit_file_type(),
                                            supportedCircuitFormats);
}

std::expected<mqss::mqssci::ResultFormat, mqss::qrmci::Error>
mqss::qrmci::BackendWrapper::compilerResultFormat() const {
  for (const auto &format : supportedCircuitFormats) {
    if (auto resultFormat = mapCircuitFormatToResultFormat(format)) {
      return *resultFormat;
    }
  }
  return std::unexpected(Error{Error::Kind::UnsupportedFormat,
                               "No compatible result format found"});
}
