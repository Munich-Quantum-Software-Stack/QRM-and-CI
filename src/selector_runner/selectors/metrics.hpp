#ifndef METRICS_HPP
#define METRICS_HPP

#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "PassRunner.hpp"

#include <llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>

using llvm::orc::ThreadSafeModule;

int evaluate_depth(ThreadSafeModule &TSM);

int evaluate_gates(ThreadSafeModule &TSM);

/*
 *   Source: "SupermarQ: A Scalable Quantum Benchmark Suite"
 *   entaglement_ratio = multi_qubit_gates / all_gates
 */
double evaluate_entanglement_ratio(ThreadSafeModule &TSM);

/*
 *   Source: "SupermarQ: A Scalable Quantum Benchmark Suite"
 *   parallelism = ((numer_of_gates / depth) - 1) * (1 / (numer_of_qubits - 1))
 */
double evaluate_parallelism(ThreadSafeModule &TSM);

/*
 *   Source: "SupermarQ: A Scalable Quantum Benchmark Suite"
 *   critical_depth = multi_qubits_gates_on_depth_path / multi_qubits_gates
 */
double evaluate_critical_depth(ThreadSafeModule &TSM);

#endif