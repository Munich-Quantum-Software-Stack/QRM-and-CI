/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/CircuitFormatPolicy.h"

#include "mqss/Protocol.hpp"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <algorithm>
#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace mqss::qrmci {

namespace {

// Fixed priority order for compilation targets. MQSS Compiler only ever
// returns textual bytes, so module variants of QIR are never selected here;
// the order itself -- not a backend's own format list order -- decides which
// target wins when several would work.
constexpr std::array<CompilerTarget, 3> CompilerTargetPriority{{
    {mqss::mqssci::ResultFormat::OPENQASM2,
     mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2, "qasm2"},
    {mqss::mqssci::ResultFormat::QIRBASE,
     mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING, "qirbase"},
    {mqss::mqssci::ResultFormat::QIRADAPTIVE,
     mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING, "qiradaptive"},
}};

// The compiler's own vocabulary for an input task circuit type. Only quake
// and catalyst circuits are compiler inputs today.
constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
    TaskCircuitTypeToCompilerInputFormatMapping{{
        {"quake", "cudaq-quake"},
        {"catalyst", "catalyst-quantum"},
    }};

} // namespace

mqss::CircuitFormat
mapTaskCircuitTypeToCircuitFormat(const std::string &format) noexcept {
  if (format == "qasm" || format == "qasm2") {
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2;
  }
  if (format == "qasm3") {
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3;
  }
  if (format == "qir" || format == "qirbase" || format == "quake" ||
      format == "catalyst") {
    // Quake/Catalyst have no distinct protocol wire-format enumerator;
    // direct submission uses QIRBASESTRING.
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING;
  }
  if (format == "qiradaptive") {
    return mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING;
  }
  return mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED;
}

std::expected<CompilerTarget, Error>
chooseCompilerTarget(std::span<const mqss::CircuitFormat> backendFormats) {
  for (const auto &target : CompilerTargetPriority) {
    if (std::ranges::contains(backendFormats, target.circuitFormat)) {
      return target;
    }
  }
  return std::unexpected(Error{Error::Kind::UnsupportedFormat,
                               "No compatible result format found"});
}

std::expected<void, Error> validateInputFormatIsSupportedCircuitFormat(
    std::string_view circuitFileFormat,
    std::span<const std::string_view> supportedInputFormats) {
  const auto inputFormat =
      std::ranges::find_if(TaskCircuitTypeToCompilerInputFormatMapping,
                           [circuitFileFormat](const auto &mapping) {
                             return mapping.first == circuitFileFormat;
                           });

  if (inputFormat != TaskCircuitTypeToCompilerInputFormatMapping.end() &&
      std::ranges::contains(supportedInputFormats, inputFormat->second)) {
    return {};
  }
  return std::unexpected(Error{Error::Kind::UnsupportedFormat,
                               "Unsupported circuit file format: " +
                                   std::string(circuitFileFormat)});
}

bool canPrepareTaskForBackend(
    const mqss::QuantumTask &task,
    std::span<const mqss::CircuitFormat> backendFormats) {
  if (task.no_modify()) {
    const auto directFormat =
        mapTaskCircuitTypeToCircuitFormat(task.circuit_file_type());
    return directFormat != mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED &&
           std::ranges::contains(backendFormats, directFormat);
  }
  const auto supportedInputFormats =
      mqss::mqssci::MQSSCompiler::getSupportedInputFormats();
  return validateInputFormatIsSupportedCircuitFormat(task.circuit_file_type(),
                                                     supportedInputFormats)
             .has_value() &&
         chooseCompilerTarget(backendFormats).has_value();
}

} // namespace mqss::qrmci
