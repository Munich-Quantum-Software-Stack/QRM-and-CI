/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file BackendWrapper.h
/// @brief One backend's capabilities as the QRM&CI pipeline sees them, read
///        either from a live device or from a published backend-status
///        message.

#pragma once

#include "mqss/Protocol.hpp"
#include "qrmci/CircuitFormatPolicy.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mqss::submitter {
class Device;
} // namespace mqss::submitter

namespace mqss::qrmci::test {
class BackendBuilder;
} // namespace mqss::qrmci::test

namespace mqss::qrmci {

/// @brief A snapshot of one backend's capabilities: name, qubit count,
///        status, supported instructions, qubit connectivity and circuit
///        formats.
///
/// The snapshot is read either from a live device through
/// mqss::submitter::Device or from an mqss::Backend status message, and it
/// answers the three questions the pipeline asks of a backend: whether it is
/// online, whether it can run a given task, and which compiler result format
/// to target for it. It is a value with no setters -- refreshing a backend
/// means building a new snapshot.
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

  /// @brief Read a backend's capabilities from a live device.
  ///
  /// A factory rather than a constructor because any of the five device
  /// property queries can fail, and a constructor has nowhere to report that
  /// -- it could only leave behind a half-populated wrapper that still looks
  /// usable. The device ID is read too, but it is a plain accessor on Device
  /// rather than a fallible query.
  /// @param device The device to read.
  /// @param dispatchQueue The queue tasks assigned to this backend should be
  ///        sent to, carried through to getQueueName()/toBackend() for a
  ///        caller that publishes this snapshot. Left empty for a caller
  ///        that never publishes -- fromPublishedStatus() still rejects an
  ///        empty queue on the receiving end, so this default is only safe
  ///        for a process that never sends its own status out.
  /// @return The populated wrapper, or the first query failure, wrapped as
  ///         Error::Kind::DeviceError.
  [[nodiscard]] static std::expected<BackendWrapper, Error>
  fromSubmitter(const mqss::submitter::Device &device,
                std::string_view dispatchQueue = "");

  /// @brief Build a validated snapshot from a published backend-status
  ///        message.
  ///
  /// The one way a published status becomes a BackendWrapper outside test
  /// code, so production can never schedule onto a backend whose status was
  /// malformed. Required routing strings (name, dispatch queue) must be
  /// non-empty; the three wire enum families (type, status, circuit formats)
  /// each normalize an enumerator this build does not know to their
  /// `*_UNSPECIFIED` value rather than propagating an out-of-range value, the
  /// same way mapProtoCircuitFormat() already did for circuit formats alone.
  /// @param backend The received backend status.
  /// @return The validated wrapper, or a MessagingFailed error naming the
  ///         invalid field.
  [[nodiscard]] static std::expected<BackendWrapper, Error>
  fromPublishedStatus(const mqss::Backend &backend);

  /// @brief Build a backend-status message from this snapshot, for
  ///        publication to other QRM&CI processes. The inverse of
  ///        BackendWrapper(const mqss::Backend &).
  /// @return The backend status to publish.
  [[nodiscard]] mqss::Backend toBackend() const;

  /// @brief Check whether this backend is currently able to accept tasks.
  /// @return True if the backend's status counts as online.
  [[nodiscard]] bool isOnline() const noexcept;

  /// @brief Check whether this backend can run a given task.
  /// @param task The task to check.
  /// @return True if the backend has enough qubits and supports a circuit
  ///         format compatible with the task's circuit file type.
  [[nodiscard]] bool canRun(const mqss::QuantumTask &task) const;

  /// @brief Find the one compiler target this backend's supported circuit
  ///        formats make available, in CircuitFormatPolicy's fixed priority
  ///        order.
  /// @return The compiler target to compile for, or an UnsupportedFormat
  ///         error if none of this backend's formats is a compiler output.
  [[nodiscard]] std::expected<CompilerTarget, Error> compilerTarget() const;

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
  /// @brief Get the backend's dispatch queue name.
  /// @return The name of the queue tasks assigned to this backend are sent
  ///         to.
  [[nodiscard]] const std::string &getQueueName() const noexcept {
    return queueName;
  }

private:
  /// @brief Build an unvalidated snapshot directly from a backend-status
  ///        message, keeping whatever it carries, valid or not, aside from
  ///        wire-enum normalization.
  ///
  /// Private so production code cannot bypass fromPublishedStatus()'s
  /// validation; test code that needs to construct backends validation would
  /// reject uses this through the befriended test::BackendBuilder instead.
  /// @param backend The received backend status.
  explicit BackendWrapper(const mqss::Backend &backend);

  friend class mqss::qrmci::test::BackendBuilder;

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
