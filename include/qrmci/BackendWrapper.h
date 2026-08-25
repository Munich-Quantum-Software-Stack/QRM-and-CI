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

#include "Submitter.h"
#include "mqss/Protocol.hpp"

#include <string>
#include <utility>
#include <vector>

namespace mqss::qrmci {

/// @brief BackendWrapper is a wrapper class for the MQSS Backend object,
///        providing a simplified interface for accessing backend properties
///        and methods. It allows for easy retrieval of backend information such
///        as name, number of qubits, type, status, queue length, current
///        load, queue name, supported instructions, qubit connectivity, and
///        supported circuit formats.
class BackendWrapper {

public:
  BackendWrapper() = default;
  BackendWrapper(const BackendWrapper &) = default;
  BackendWrapper(BackendWrapper &&) = default;
  BackendWrapper &operator=(const BackendWrapper &) = default;
  BackendWrapper &operator=(BackendWrapper &&) = default;

  /// @brief Construct a BackendWrapper from a Submitter object. This
  /// constructor initializes the wrapper with the backend information retrieved
  /// from the Submitter.
  /// @param submitter The Submitter object from which to retrieve backend
  ///        information.
  explicit BackendWrapper(mqss::submitter::Submitter &submitter);

  /// @brief Construct a BackendWrapper from a MQSS Backend object. This
  /// constructor initializes the wrapper with the backend information retrieved
  /// from the MQSS Backend object.
  /// @param backend The MQSS Backend object from which to retrieve backend
  ///        information.
  explicit BackendWrapper(const mqss::Backend &backend);

  /// @brief Create a MQSS Backend object from the information stored in the
  ///        BackendWrapper. This method allows for easy conversion back to the
  ///        original MQSS Backend object.
  /// @return A MQSS Backend object constructed from the information stored in
  ///         the BackendWrapper.
  [[nodiscard]] mqss::Backend makeBackend();

  /// @brief Accessor methods to retrieve backend information
  [[nodiscard]] const std::string &getName() const noexcept { return name; }
  [[nodiscard]] std::uint32_t getNumQubits() const noexcept {
    return numQubits;
  }
  [[nodiscard]] mqss::BackendType getType() const noexcept { return type; }
  [[nodiscard]] mqss::BackendStatus getStatus() const noexcept {
    return status;
  }
  [[nodiscard]] std::uint32_t getQueueLength() const noexcept {
    return queueLength;
  }
  [[nodiscard]] float getCurrentLoad() const noexcept { return currentLoad; }
  [[nodiscard]] const std::string &getQueueName() const noexcept {
    return queueName;
  }
  [[nodiscard]] const std::vector<std::string> &
  getInstructions() const noexcept {
    return instructions;
  }
  [[nodiscard]] const std::vector<std::pair<std::uint32_t, std::uint32_t>> &
  getQubitConnectivity() const noexcept {
    return qubitConnectivity;
  }
  [[nodiscard]] const std::vector<mqss::CircuitFormat> &
  getSupportedCircuitFormats() const noexcept {
    return supportedCircuitFormats;
  }

  /// @brief Mutator methods to set backend information
  void setName(const std::string &newName) { name = newName; }
  void setNumQubits(std::uint32_t newNumQubits) { numQubits = newNumQubits; }
  void setType(mqss::BackendType newType) { type = newType; }
  void setStatus(mqss::BackendStatus newStatus) { status = newStatus; }
  void setQueueLength(std::uint32_t newQueueLength) {
    queueLength = newQueueLength;
  }
  void setCurrentLoad(float newCurrentLoad) { currentLoad = newCurrentLoad; }
  void setQueueName(const std::string &newQueueName) {
    queueName = newQueueName;
  }
  void setInstructions(const std::vector<std::string> &newInstructions) {
    instructions = newInstructions;
  }
  void setQubitConnectivity(
      const std::vector<std::pair<std::uint32_t, std::uint32_t>>
          &newQubitConnectivity) {
    qubitConnectivity = newQubitConnectivity;
  }
  void setSupportedCircuitFormats(
      const std::vector<mqss::CircuitFormat> &newSupportedCircuitFormats) {
    supportedCircuitFormats = newSupportedCircuitFormats;
  }

private:
  std::string name;
  std::uint32_t numQubits;
  mqss::BackendType type;
  mqss::BackendStatus status;
  std::uint32_t queueLength;
  float currentLoad;
  std::string queueName;
  std::vector<std::string> instructions;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> qubitConnectivity;
  std::vector<mqss::CircuitFormat> supportedCircuitFormats;
};
} // namespace mqss::qrmci
