/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "mqss/Protocol.hpp"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <unordered_map>
#include <unordered_set>

const std::unordered_map<int, mqss::mqssci::OptLevel>
    IntToCompilerOptLevelMapping = {
        {1, mqss::mqssci::OptLevel::O1},
        {2, mqss::mqssci::OptLevel::O2},
        {3, mqss::mqssci::OptLevel::O3},
};

const std::unordered_multimap<mqss::CircuitFormat, mqss::mqssci::ResultFormat>
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

const std::unordered_multimap<std::string, mqss::CircuitFormat>
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
