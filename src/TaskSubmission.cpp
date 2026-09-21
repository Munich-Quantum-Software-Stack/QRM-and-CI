/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "TaskSubmission.h"

#include "qrmci/CircuitFormatPolicy.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mqss::qrmci::detail {

std::expected<mqss::CircuitFormat, Error>
validateSubmission(const mqss::QuantumTask &task) {
  // Refused here rather than at the device: Device::submitJob makes no
  // promise about either case, and a task with nothing to run or nothing to
  // measure is the caller's input being wrong, not the device faulting.
  if (task.circuit_files().empty()) {
    return std::unexpected(Error{Error::Kind::SubmissionFailed,
                                 "Task carries no circuit files to submit."});
  }
  if (task.n_shots() <= 0) {
    return std::unexpected(
        Error{Error::Kind::SubmissionFailed,
              "Task requests a non-positive number of shots."});
  }

  const auto format =
      mapTaskCircuitTypeToCircuitFormat(task.circuit_file_type());
  if (format == mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED) {
    return std::unexpected(
        Error{Error::Kind::SubmissionFailed,
              "Task's circuit file type has no known submission format: " +
                  task.circuit_file_type()});
  }
  return format;
}

mqss::submitter::JobRequest makeJobRequest(const mqss::QuantumTask &task,
                                           int circuitIndex,
                                           mqss::CircuitFormat format) {
  const auto &circuit = task.circuit_files(circuitIndex);
  std::vector<std::byte> payload(circuit.size());
  std::ranges::transform(circuit, payload.begin(), [](char value) {
    return static_cast<std::byte>(value);
  });
  return {.format = format,
          .payload = std::move(payload),
          .numShots = static_cast<std::uint64_t>(task.n_shots())};
}

} // namespace mqss::qrmci::detail
