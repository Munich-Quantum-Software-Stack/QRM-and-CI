/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/ConstantsMapping.h"

#include "mqss/Protocol.hpp"
#include "qrmci/Error.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {

// The lookup tables backing the translations below. They are deliberately
// confined to this translation unit: the functions in ConstantsMapping.h are
// the whole interface, so a table can change shape without any consumer of
// the shipped headers noticing.

const std::unordered_map<int, mqss::mqssci::OptLevel>
    IntToCompilerOptLevelMapping = {
        {1, mqss::mqssci::OptLevel::O1},
        {2, mqss::mqssci::OptLevel::O2},
        {3, mqss::mqssci::OptLevel::O3},
};

// Keys are unique -- one circuit format is produced by exactly one compiler
// result format -- so this is a map, looked up with find().
const std::unordered_map<mqss::CircuitFormat, mqss::mqssci::ResultFormat>
    CircuitFormatToResultFormatMapping = {
        {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2,
         mqss::mqssci::ResultFormat::OPENQASM2},
        {mqss::CircuitFormat::CIRCUIT_FORMAT_QIR,
         mqss::mqssci::ResultFormat::QIR},
        {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING,
         mqss::mqssci::ResultFormat::QIRBASE},
        {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE,
         mqss::mqssci::ResultFormat::QIRBASE},
        {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING,
         mqss::mqssci::ResultFormat::QIRADAPTIVE},
        {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE,
         mqss::mqssci::ResultFormat::QIRADAPTIVE},
};

const std::unordered_map<std::string_view, std::string_view>
    TaskCircuitTypeToCompilerInputFormatMapping = {
        {"quake", "cudaq-quake"},
        {"catalyst", "catalyst-quantum"},
};

// One circuit type maps to several acceptable circuit formats, so this one is
// genuinely a multimap and is walked as an equal_range.
const std::unordered_multimap<std::string_view, mqss::CircuitFormat>
    TaskCircuitTypeToCompatibleCircuitFormatMapping = {
        {"qasm", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2},
        {"qasm2", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2},
        {"qasm3", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM3},
        {"qir", mqss::CircuitFormat::CIRCUIT_FORMAT_QIR},
        {"qir", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        {"qir", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE},
        {"qir", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING},
        {"qir", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE},
        {"qirbase", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        {"qirbase", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE},
        {"qiradaptive", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING},
        {"qiradaptive", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE},
        {"quake", mqss::CircuitFormat::CIRCUIT_FORMAT_QIR},
        {"quake", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        {"quake", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE},
        {"quake", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING},
        {"quake", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE},
        {"quake", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2},
        {"catalyst", mqss::CircuitFormat::CIRCUIT_FORMAT_QIR},
        {"catalyst", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING},
        {"catalyst", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASEMODULE},
        {"catalyst", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVESTRING},
        {"catalyst", mqss::CircuitFormat::CIRCUIT_FORMAT_QIRADAPTIVEMODULE},
        {"catalyst", mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2},
};

const std::unordered_set<mqss::BackendStatus> OnlineBackendStatuses = {
    mqss::BackendStatus::BACKEND_STATUS_IDLE,
    mqss::BackendStatus::BACKEND_STATUS_BUSY,
};

} // namespace

mqss::BackendStatus
mqss::qrmci::mapBackendStatus(QDMI_Device_Status status) noexcept {
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

mqss::CircuitFormat
mqss::qrmci::mapCircuitFormat(QDMI_Program_Format format) noexcept {
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

mqss::CircuitFormat mqss::qrmci::mapProtoCircuitFormat(int format) noexcept {
  // protobuf keeps an enum value it does not recognise as its raw integer
  // rather than rejecting the message, so a straight static_cast here would
  // manufacture an out-of-range mqss::CircuitFormat and feed it into the
  // compatibility comparisons.
  if (!mqss::protocol::v1::CircuitFormat_IsValid(format)) {
    return mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED;
  }
  return static_cast<mqss::CircuitFormat>(format);
}

mqss::mqssci::OptLevel mqss::qrmci::getCompilerOptimizationLevel(int level) {
  auto it = IntToCompilerOptLevelMapping.find(level);
  if (it != IntToCompilerOptLevelMapping.end()) {
    return it->second;
  }
  return mqss::mqssci::OptLevel::O3; // Default to O3 if invalid level
}

bool mqss::qrmci::isOnlineBackendStatus(mqss::BackendStatus status) noexcept {
  return OnlineBackendStatuses.contains(status);
}

std::optional<mqss::mqssci::ResultFormat>
mqss::qrmci::mapCircuitFormatToResultFormat(
    mqss::CircuitFormat format) noexcept {
  auto it = CircuitFormatToResultFormatMapping.find(format);
  if (it == CircuitFormatToResultFormatMapping.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool mqss::qrmci::isCircuitTypeCompatibleWithFormats(
    std::string_view circuitFileType,
    std::span<const mqss::CircuitFormat> supportedCircuitFormats) {
  auto [first, last] =
      TaskCircuitTypeToCompatibleCircuitFormatMapping.equal_range(
          circuitFileType);
  return std::any_of(first, last, [supportedCircuitFormats](const auto &entry) {
    return std::ranges::find(supportedCircuitFormats, entry.second) !=
           supportedCircuitFormats.end();
  });
}

std::expected<void, mqss::qrmci::Error>
mqss::qrmci::validateInputFormatIsSupportedCircuitFormat(
    std::string_view circuitFileFormat,
    std::span<const std::string_view> supportedInputFormats) {
  auto it = TaskCircuitTypeToCompilerInputFormatMapping.find(circuitFileFormat);
  if (it != TaskCircuitTypeToCompilerInputFormatMapping.end()) {
    if (std::ranges::find(supportedInputFormats, it->second) !=
        supportedInputFormats.end()) {
      return {};
    }
  }
  return std::unexpected(Error{Error::Kind::UnsupportedFormat,
                               "Unsupported circuit file format: " +
                                   std::string(circuitFileFormat)});
}
