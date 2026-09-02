/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file BackendWrapperTestBuilder.h
/// @brief Test-only builder for BackendWrapper. BackendWrapper has no
///        setters -- it is built from a live submitter or from a wire
///        message and is immutable afterwards -- so tests that need a
///        backend with specific properties assemble the mqss::Backend
///        message first and wrap that. Keeping the builder here rather than
///        in the production interface is the point: the mutable surface
///        exists only for tests.

#pragma once

#include "mqss/Protocol.hpp"
#include "qrmci/BackendWrapper.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mqss::qrmci::test {

class BackendBuilder {
public:
  BackendBuilder &name(const std::string &value) {
    backend.set_name(value);
    return *this;
  }
  BackendBuilder &numQubits(std::uint32_t value) {
    backend.set_num_qubits(value);
    return *this;
  }
  BackendBuilder &type(mqss::BackendType value) {
    backend.set_type(value);
    return *this;
  }
  BackendBuilder &status(mqss::BackendStatus value) {
    backend.set_status(value);
    return *this;
  }
  BackendBuilder &queueLength(std::uint32_t value) {
    backend.set_queue_length(value);
    return *this;
  }
  BackendBuilder &currentLoad(float value) {
    backend.set_current_load(value);
    return *this;
  }
  BackendBuilder &queueName(const std::string &value) {
    backend.set_queue_name(value);
    return *this;
  }
  BackendBuilder &instructions(const std::vector<std::string> &values) {
    backend.clear_instructions();
    for (const auto &value : values) {
      backend.add_instructions(value);
    }
    return *this;
  }
  BackendBuilder &qubitConnectivity(
      const std::vector<std::pair<std::uint32_t, std::uint32_t>> &values) {
    backend.clear_connectivity();
    for (const auto &[first, second] : values) {
      auto *qpair = backend.add_connectivity();
      qpair->set_qubit1(first);
      qpair->set_qubit2(second);
    }
    return *this;
  }
  BackendBuilder &
  supportedCircuitFormats(const std::vector<mqss::CircuitFormat> &values) {
    backend.clear_supported_circuit_formats();
    for (const auto &value : values) {
      backend.add_supported_circuit_formats(value);
    }
    return *this;
  }
  /// @brief Add a raw circuit-format value, including one that is not a
  ///        known enumerator, the way a peer on a newer protocol version
  ///        would put it on the wire.
  BackendBuilder &rawSupportedCircuitFormat(int value) {
    backend.add_supported_circuit_formats(
        static_cast<mqss::CircuitFormat>(value));
    return *this;
  }

  /// @brief The assembled protobuf message.
  [[nodiscard]] const mqss::Backend &proto() const { return backend; }
  /// @brief The BackendWrapper built from the assembled message.
  [[nodiscard]] BackendWrapper build() const { return BackendWrapper(backend); }

private:
  mqss::Backend backend;
};

} // namespace mqss::qrmci::test
