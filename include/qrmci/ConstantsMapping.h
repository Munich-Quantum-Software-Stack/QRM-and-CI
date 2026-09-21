/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file ConstantsMapping.h
/// @brief Pure translations between the submitter, mqss protocol and mqss-ci
///        enumerations: status, optimisation level, and wire-format decoding.
///        The tables themselves are private to src/ConstantsMapping.cpp;
///        these functions are the whole interface. Task/compiler/backend
///        circuit-format admission policy lives in CircuitFormatPolicy.h
///        instead -- that is policy, not translation.
///
/// No QDMI type appears here. QDMI translation belongs to MQSS-Submitter,
/// behind its Client/Device/Job interface, which is what keeps QDMI out of
/// every QRM&CI public header.

#pragma once

#include "mqss/Protocol.hpp"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <expected>
#include <mqss/submitter/Error.h>
#include <string>

namespace mqss::qrmci {

/// @brief Wrap a submitter failure as a QRM&CI failure of a kind the caller
///        names.
///
/// The submitter's ErrorCode is deliberately generic -- it cannot distinguish
/// a refused submission from a device fault -- so the caller, which knows what
/// it was trying to do, supplies the Kind. This function only formats the
/// detail: the error code's own name followed by the submitter's message.
/// @param kind The QRM&CI error kind this failure should carry.
/// @param error The submitter error to wrap.
/// @return A QRM&CI error of kind @p kind, detailing @p error.
[[nodiscard]] Error toQrmciError(Error::Kind kind,
                                 const mqss::submitter::Error &error);

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

/// @brief Translate a raw backend-type value received over the wire into a
///        mqss::BackendType, the same way mapProtoCircuitFormat() handles
///        circuit formats: an enumerator this build does not know becomes
///        BACKEND_TYPE_UNSPECIFIED rather than an out-of-range value.
/// @param type The raw backend-type value as carried in the message.
/// @return The matching mqss::BackendType, or
///         mqss::BackendType::BACKEND_TYPE_UNSPECIFIED if @p type is not a
///         known enumerator.
[[nodiscard]] mqss::BackendType mapProtoBackendType(int type) noexcept;

/// @brief Translate a raw backend-status value received over the wire into a
///        mqss::BackendStatus, the same way mapProtoCircuitFormat() handles
///        circuit formats: an enumerator this build does not know becomes
///        BACKEND_STATUS_UNSPECIFIED -- and therefore not online -- rather
///        than an out-of-range value.
/// @param status The raw backend-status value as carried in the message.
/// @return The matching mqss::BackendStatus, or
///         mqss::BackendStatus::BACKEND_STATUS_UNSPECIFIED if @p status is
///         not a known enumerator.
[[nodiscard]] mqss::BackendStatus mapProtoBackendStatus(int status) noexcept;

/// @brief Translate a task's requested optimisation level into the matching
///        mqss-ci compiler optimisation level. 0 and 1 both select O1 --
///        proto3's implicit zero for an omitted field must not be mistaken
///        for a request to maximize optimisation.
/// @param level The optimisation level requested on the QuantumTask.
/// @return The matching mqss::mqssci::OptLevel, or a CompilationFailed error
///         naming @p level if it is outside 0-3.
[[nodiscard]] std::expected<mqss::mqssci::OptLevel, Error>
getCompilerOptimizationLevel(int level);

/// @brief Check whether a backend status means the backend is able to accept
///        tasks.
/// @param status The backend status to check.
/// @return True for the statuses that count as online (idle and busy).
[[nodiscard]] bool isOnlineBackendStatus(mqss::BackendStatus status) noexcept;

/// @brief Name a circuit format for a log line.
/// @param format The circuit format to describe.
/// @return The protocol's stable enumerator name (e.g. `CIRCUIT_FORMAT_QASM2`),
///         or `UNKNOWN(<number>)` if @p format is not a known enumerator --
///         the same case mapProtoCircuitFormat() normalizes away before this
///         ever sees it, kept here only so a raw value logged before
///         normalization still reads clearly.
[[nodiscard]] std::string circuitFormatLabel(mqss::CircuitFormat format);

/// @brief Name a backend status for a log line.
/// @param status The backend status to describe.
/// @return The protocol's stable enumerator name (e.g. `BACKEND_STATUS_IDLE`),
///         or `UNKNOWN(<number>)` if @p status is not a known enumerator.
[[nodiscard]] std::string backendStatusLabel(mqss::BackendStatus status);

} // namespace mqss::qrmci
