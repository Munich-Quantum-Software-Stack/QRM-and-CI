/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file BackendWrapper.h
/// @brief A wrapper class for the MQSS Backend object, providing a simplified
///        interface for accessing backend properties and methods.

#pragma once

#include "mqss/Protocol.hpp"
#include "qdmi/constants.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace mqss::submitter {
class Submitter;
} // namespace mqss::submitter

namespace mqss::qrmci {

/// @brief BackendWrapper is a wrapper class for the MQSS Backend object. It
///        holds a backend's capabilities -- name, qubit count, status,
///        supported instructions, qubit connectivity and circuit formats --
///        as read from a live QDMI device or from a backend-status message,
///        and answers the questions the pipeline asks of a backend: whether
///        it is online, whether it can run a given task, and which compiler
///        result format to target for it.
class BackendWrapper {

public:
  /// @brief Default-construct an empty BackendWrapper.
  BackendWrapper() = default;
  /// @brief Copy-construct a BackendWrapper.
  BackendWrapper(const BackendWrapper &) = default;
  /// @brief Move-construct a BackendWrapper.
  BackendWrapper(BackendWrapper &&) = default;
  /// @brief Copy-assign a BackendWrapper.
  BackendWrapper &operator=(const BackendWrapper &) = default;
  /// @brief Move-assign a BackendWrapper.
  BackendWrapper &operator=(BackendWrapper &&) = default;

  /// @brief Construct a BackendWrapper from a submitter object. This
  /// constructor initializes the wrapper with the backend information retrieved
  /// from the submitter.
  /// @param submitter The submitter object from which to retrieve backend
  ///        information.
  explicit BackendWrapper(mqss::submitter::Submitter &submitter);

  /// @brief Construct a BackendWrapper from a MQSS Backend object. This
  /// constructor initializes the wrapper with the backend information retrieved
  /// from the MQSS Backend object. Circuit formats arrive over the wire and
  /// are translated through mapProtoCircuitFormat(), so an enumerator this
  /// build does not know becomes CIRCUIT_FORMAT_UNSPECIFIED rather than an
  /// out-of-range value.
  /// @param backend The MQSS Backend object from which to retrieve backend
  ///        information.
  explicit BackendWrapper(const mqss::Backend &backend);

  /// @brief Create a MQSS Backend object from the information stored in the
  ///        BackendWrapper, for publication to other QRM&CI processes. This
  ///        is the inverse of BackendWrapper(const mqss::Backend &).
  /// @return A MQSS Backend object constructed from the information stored in
  ///         the BackendWrapper.
  [[nodiscard]] mqss::Backend toBackend() const;

  /// @brief Check whether this backend is currently able to accept tasks.
  /// @return True if the backend's status counts as online.
  [[nodiscard]] bool isOnline() const noexcept;

  /// @brief Check whether this backend can run a given task.
  /// @param task The task to check.
  /// @return True if the backend has enough qubits and supports a circuit
  ///         format compatible with the task's circuit file type.
  [[nodiscard]] bool canRun(const mqss::QuantumTask &task) const;

  /// @brief Find a compiler result format supported by both this backend and
  ///        the mqss-ci compiler.
  /// @return The first backend-supported circuit format that has a known
  ///         result-format mapping, or an UnsupportedFormat error if none is
  ///         found.
  [[nodiscard]] std::expected<mqss::mqssci::ResultFormat, Error>
  compilerResultFormat() const;

  /// @brief Get the backend name.
  /// @return The backend name.
  [[nodiscard]] const std::string &getName() const noexcept { return name; }
  /// @brief Get the number of qubits supported by the backend.
  /// @return The number of qubits.
  [[nodiscard]] std::uint32_t getNumQubits() const noexcept {
    return numQubits;
  }
  /// @brief Get the backend status.
  /// @return The backend status.
  [[nodiscard]] mqss::BackendStatus getStatus() const noexcept {
    return status;
  }
  /// @brief Get the instructions supported by the backend.
  /// @return The supported instructions.
  [[nodiscard]] const std::vector<std::string> &
  getInstructions() const noexcept {
    return instructions;
  }
  /// @brief Get the qubit connectivity of the backend.
  /// @return The qubit connectivity as a list of connected qubit index
  ///         pairs.
  [[nodiscard]] const std::vector<std::pair<std::uint32_t, std::uint32_t>> &
  getQubitConnectivity() const noexcept {
    return qubitConnectivity;
  }
  /// @brief Get the circuit formats supported by the backend.
  /// @return The supported circuit formats.
  [[nodiscard]] const std::vector<mqss::CircuitFormat> &
  getSupportedCircuitFormats() const noexcept {
    return supportedCircuitFormats;
  }

private:
  std::string name;
  std::uint32_t numQubits{};
  mqss::BackendType type{};
  mqss::BackendStatus status{};
  std::uint32_t queueLength{};
  float currentLoad{};
  std::string queueName;
  std::vector<std::string> instructions;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> qubitConnectivity;
  std::vector<mqss::CircuitFormat> supportedCircuitFormats;
};
} // namespace mqss::qrmci
