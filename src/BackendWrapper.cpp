/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/BackendWrapper.h"

#include "mqss/Protocol.hpp"
#include "qrmci/CircuitFormatPolicy.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <mqss/submitter/Device.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mqss::qrmci {

std::expected<BackendWrapper, Error>
BackendWrapper::fromSubmitter(const mqss::submitter::Device &device,
                              std::string_view dispatchQueue) {
  // Queried in a fixed order, first failure wins: a half-read device is not a
  // backend the pipeline should be allowed to schedule onto. deviceId() is not
  // among them -- it is the caller-supplied Device ID (defaulting to the
  // device's own name), returned by a plain accessor that cannot fail.
  const auto asDeviceError = [](const mqss::submitter::Error &error) {
    return toQrmciError(Error::Kind::DeviceError, error);
  };

  auto numQubits = device.qubitsNum().transform_error(asDeviceError);
  if (!numQubits) {
    return std::unexpected(std::move(numQubits.error()));
  }
  auto status = device.status().transform_error(asDeviceError);
  if (!status) {
    return std::unexpected(std::move(status.error()));
  }
  auto instructions = device.operationNames().transform_error(asDeviceError);
  if (!instructions) {
    return std::unexpected(std::move(instructions.error()));
  }
  auto connectivity = device.couplingMap().transform_error(asDeviceError);
  if (!connectivity) {
    return std::unexpected(std::move(connectivity.error()));
  }
  auto formats =
      device.supportedPayloadFormats().transform_error(asDeviceError);
  if (!formats) {
    return std::unexpected(std::move(formats.error()));
  }

  BackendWrapper backend;
  backend.name = std::string(device.deviceId());
  backend.numQubits = static_cast<std::uint32_t>(*numQubits);
  backend.type = mqss::BackendType::BACKEND_TYPE_UNSPECIFIED;
  // No translation: Device reports mqss::BackendStatus itself.
  backend.status = *status;
  backend.queueName = std::string(dispatchQueue);
  backend.instructions = std::move(*instructions);

  backend.qubitConnectivity.reserve(connectivity->size());
  for (const auto &[firstQubit, secondQubit] : *connectivity) {
    backend.qubitConnectivity.emplace_back(
        static_cast<std::uint32_t>(firstQubit),
        static_cast<std::uint32_t>(secondQubit));
  }

  // No translation here either: supportedPayloadFormats() already reports
  // mqss::CircuitFormat.
  backend.supportedCircuitFormats = std::move(*formats);

  return backend;
}

BackendWrapper::BackendWrapper(const mqss::Backend &backend)
    : name(backend.name()), numQubits(backend.num_qubits()),
      type(mapProtoBackendType(static_cast<int>(backend.type()))),
      status(mapProtoBackendStatus(static_cast<int>(backend.status()))),
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

std::expected<BackendWrapper, Error>
BackendWrapper::fromPublishedStatus(const mqss::Backend &backend) {
  // Required routing strings: a name selects the entry a task lands on, a
  // queue name is where a chosen task is actually sent. Neither can be
  // recovered downstream, so a message missing either is rejected outright
  // rather than registered under a name or queue nothing dispatches to.
  if (backend.name().empty()) {
    return std::unexpected(
        Error{Error::Kind::MessagingFailed,
              "Published backend status has an empty name."});
  }
  if (backend.queue_name().empty()) {
    return std::unexpected(
        Error{Error::Kind::MessagingFailed, "Published backend status for '" +
                                                backend.name() +
                                                "' has an empty queue_name."});
  }
  return BackendWrapper(backend);
}

mqss::Backend BackendWrapper::toBackend() const {
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
  for (const auto &[firstQubit, secondQubit] : qubitConnectivity) {
    mqss::QubitPair *qpair = backend.add_connectivity();
    qpair->set_qubit1(firstQubit);
    qpair->set_qubit2(secondQubit);
  }
  for (const auto &format : supportedCircuitFormats) {
    backend.add_supported_circuit_formats(format);
  }

  return backend;
}

bool BackendWrapper::isOnline() const noexcept {
  return isOnlineBackendStatus(status);
}

bool BackendWrapper::canRun(const mqss::QuantumTask &task) const {
  if (task.n_qbits() < 0 ||
      static_cast<std::uint32_t>(task.n_qbits()) > numQubits) {
    return false;
  }
  // An empty list means unrestricted; a non-empty one is an allow-list of
  // backend IDs the task may run on, checked before format admission so a
  // restricted task never runs on a backend it merely happens to be
  // compatible with.
  if (!task.restricted_resource_names().empty() &&
      !std::ranges::contains(task.restricted_resource_names(), name)) {
    return false;
  }
  return canPrepareTaskForBackend(task, supportedCircuitFormats);
}

std::expected<CompilerTarget, Error> BackendWrapper::compilerTarget() const {
  return chooseCompilerTarget(supportedCircuitFormats);
}

} // namespace mqss::qrmci
