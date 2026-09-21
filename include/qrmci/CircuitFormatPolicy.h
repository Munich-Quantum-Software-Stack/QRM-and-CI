/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file CircuitFormatPolicy.h
/// @brief The one policy that decides whether a task is directly submittable
///        or compilable for a backend, and which concrete circuit format
///        and compiler result format that choice means. Kept separate from
///        ConstantsMapping.h: this is task/compiler/backend admission
///        policy, not a translation between two enumerations.

#pragma once

#include "mqss/Protocol.hpp"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace mqss::qrmci {

/// @brief One deterministic compilation target: the mqss-ci ResultFormat the
///        compiler must be asked to emit, the exact protocol CircuitFormat
///        that output is submitted as, and the canonical task circuit-file
///        type label a successfully compiled task should carry afterward.
struct CompilerTarget {
  mqss::mqssci::ResultFormat compilerFormat;
  mqss::CircuitFormat circuitFormat;
  std::string_view taskCircuitType;
};

/// @brief Translate a task circuit type into its one direct circuit format,
///        for no-modify submission or for a task's already-canonical
///        circuit_file_type after a successful compilation.
///
/// An exhaustive, deterministic switch: qasm/qasm2 -> QASM2, qasm3 -> QASM3,
/// qir/qirbase -> QIRBASESTRING, qiradaptive -> QIRADAPTIVESTRING, and
/// quake/catalyst -> QIRBASESTRING for direct submission because these task
/// circuit types have no distinct protocol wire-format enumerator.
/// @param format The task circuit type to translate (e.g. "qasm3").
/// @return The matching mqss::CircuitFormat, or
///         mqss::CircuitFormat::CIRCUIT_FORMAT_UNSPECIFIED if @p format has
///         no known direct mapping.
[[nodiscard]] mqss::CircuitFormat
mapTaskCircuitTypeToCircuitFormat(const std::string &format) noexcept;

/// @brief Choose the one compiler target a backend's supported circuit
///        formats make available.
///
/// Checked in this fixed priority order -- QASM2, QIRBASESTRING,
/// QIRADAPTIVESTRING -- never the module variants, since MQSS Compiler only
/// ever returns textual bytes. The backend's own format list order has no
/// effect on the outcome.
/// @param backendFormats The backend's own supported circuit formats.
/// @return The one compiler target to compile for, or an UnsupportedFormat
///         error if none of @p backendFormats is a compiler output.
[[nodiscard]] std::expected<CompilerTarget, Error>
chooseCompilerTarget(std::span<const mqss::CircuitFormat> backendFormats);

/// @brief Check whether a task can be prepared -- directly submitted or
///        compiled -- for a backend supporting @p backendFormats.
///
/// A no_modify task must already resolve, via
/// mapTaskCircuitTypeToCircuitFormat(), to one of @p backendFormats.
/// Otherwise the task's circuit type must be a known compiler input format
/// and @p backendFormats must yield a chooseCompilerTarget(). A task with
/// no_modify=false and a non-compiler input therefore fails here, at
/// admission, rather than later at compilation.
/// @param task The task to check.
/// @param backendFormats The backend's own supported circuit formats.
/// @return True if the task is either directly submittable or compilable for
///         a backend supporting @p backendFormats.
[[nodiscard]] bool
canPrepareTaskForBackend(const mqss::QuantumTask &task,
                         std::span<const mqss::CircuitFormat> backendFormats);

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
