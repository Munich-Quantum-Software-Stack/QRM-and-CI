/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file ConstantsMapping.h
/// @brief Pure translations between the QDMI, mqss protocol and mqss-ci
///        enumerations, plus the compatibility questions answered from the
///        same lookup tables. The tables themselves are private to
///        src/ConstantsMapping.cpp; these functions are the whole interface.

#pragma once

#include "mqss/Protocol.hpp"
#include "qdmi/constants.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mqss::qrmci {

/// @brief Translate a QDMI device status into the corresponding mqss
///        protocol backend status.
/// @param status The QDMI device status to translate.
/// @return The matching mqss::BackendStatus, or
///         mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED if @p status has
///         no known mapping.
[[nodiscard]] mqss::BackendStatus
mapBackendStatus(QDMI_Device_Status status) noexcept;

/// @brief Translate a QDMI program format into the corresponding mqss
///        protocol circuit format.
/// @param format The QDMI program format to translate.
/// @return The matching mqss::CircuitFormat, or
///         mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED if @p format has
///         no known mapping.
[[nodiscard]] mqss::CircuitFormat
mapCircuitFormat(QDMI_Program_Format format) noexcept;

/// @brief Translate a raw circuit-format value received over the wire into a
///        mqss::CircuitFormat. protobuf preserves unknown enum values as
///        their raw integer, so a peer built against a newer protocol can
///        hand us an enumerator this build does not know; this is the only
///        supported way to turn such a value into a mqss::CircuitFormat.
/// @param format The raw circuit-format value as carried in the message.
/// @return The matching mqss::CircuitFormat, or
///         mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED if @p format is
///         not a known enumerator.
[[nodiscard]] mqss::CircuitFormat mapProtoCircuitFormat(int format) noexcept;

/// @brief Translate a task's requested optimisation level into the matching
///        mqss-ci compiler optimisation level.
/// @param level The optimisation level requested on the QuantumTask.
/// @return The matching mqss::mqssci::OptLevel, or
///         mqss::mqssci::OptLevel::O3 if @p level has no known mapping.
[[nodiscard]] mqss::mqssci::OptLevel getCompilerOptimizationLevel(int level);

/// @brief Check whether a backend status means the backend is able to accept
///        tasks.
/// @param status The backend status to check.
/// @return True for the statuses that count as online (idle and busy).
[[nodiscard]] bool isOnlineBackendStatus(mqss::BackendStatus status) noexcept;

/// @brief Translate a circuit format into the mqss-ci compiler result format
///        that produces it.
/// @param format The circuit format to translate.
/// @return The matching mqss::mqssci::ResultFormat, or std::nullopt if the
///         compiler cannot emit @p format.
[[nodiscard]] std::optional<mqss::mqssci::ResultFormat>
mapCircuitFormatToResultFormat(mqss::CircuitFormat format) noexcept;

/// @brief Check whether a task's circuit file type can run in any of the
///        given circuit formats.
/// @param circuitFileType The task's circuit file type (e.g. "qasm3").
/// @param supportedCircuitFormats The circuit formats to match against,
///        typically a backend's.
/// @return True if at least one of @p supportedCircuitFormats is compatible
///         with @p circuitFileType.
[[nodiscard]] bool isCircuitTypeCompatibleWithFormats(
    std::string_view circuitFileType,
    std::span<const mqss::CircuitFormat> supportedCircuitFormats);

/// @brief Check that a task's circuit file format has a known mapping to one
///        of the compiler's supported input formats.
/// @param circuitFileFormat The task's circuit file format.
/// @param supportedInputFormats The compiler's supported input formats.
/// @return Nothing on success, or an UnsupportedFormat error if @p
///         circuitFileFormat is not a supported circuit format.
[[nodiscard]] std::expected<void, Error>
validateInputFormatIsSupportedCircuitFormat(
    std::string_view circuitFileFormat,
    std::span<const std::string_view> supportedInputFormats);

} // namespace mqss::qrmci
