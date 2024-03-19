/* Routine for evaluating population members  */

#include <iostream>
#include <map>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "eval.hpp"
#include "pass_runner/PassRunner.hpp"

#include <llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>

using llvm::orc::ThreadSafeContext;
using llvm::orc::ThreadSafeModule;

/*
 * @brief Extract supermarq+ features from a quantum circuit
 * @param TSM The quantum circuit to evaluate
 * @param gate_counts The counts of each gate in the circuit
 * @return A vector of original supermarq and 3 additional features
 */
std::vector<double>
evaluate_supermarq_plus(ThreadSafeModule &TSM,
                        std::map<std::string, int> &gate_counts)
{
    std::string QIS_START = "__quantum__qis_";
    int num_gates = 0, num_two_qubit_gates = 0;
    std::unordered_map<std::string, int> qubit_counts, two_qubit_gate_counts;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        qubit_connections, di_qubit_connections;

    if (!TSM)
    {
        std::cerr << "ThreadSafeModule is null" << std::endl;
        return std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    }
    TSM.withModuleDo(
        [&](Module &module)
        {
            LLVMContext &Context = module.getContext();
            StructType *qubitType = StructType::getTypeByName(Context, "Qubit");
            for (auto &function : module)
            {
                for (auto &block : function)
                {
                    for (auto &instruction : block)
                    {
                        if (auto call_instr = dyn_cast<CallBase>(&instruction))
                        {
                            if (auto f = call_instr->getCalledFunction())
                            {
                                auto op_name =
                                    static_cast<std::string>(f->getName());

                                bool is_quantum =
                                    (op_name.size() >= QIS_START.size() &&
                                     op_name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
                                    std::string prev_qubit = "";
                                    std::string first_qubit = "";

                                    // Count each gate
                                    if (gate_counts.count(op_name) >= 0)
                                    {
                                        gate_counts[op_name]++;
                                        num_gates++;
                                    }
                                    else
                                    {
                                        std::cerr << "Unknown gate: " << op_name
                                                  << std::endl;
                                    }
                                    // Look for qubits affected by the gate
                                    for (Use &operand : call_instr->operands())
                                    {
                                        if (auto *val =
                                                dyn_cast<Value>(&operand))
                                        {
                                            if (val->getType() ==
                                                PointerType::get(qubitType, 0))
                                            {
                                                std::string qubit;
                                                llvm::raw_string_ostream stream(
                                                    qubit);
                                                operand.get()->printAsOperand(
                                                    stream, true);
                                                stream.flush();
                                                // Count operations on each
                                                // qubit
                                                qubit_counts[qubit]++;

                                                // Count connections between
                                                // qubits
                                                if (prev_qubit != "")
                                                {
                                                    // Undirected graph
                                                    qubit_connections[qubit]
                                                        .insert(prev_qubit);
                                                    qubit_connections
                                                        [prev_qubit]
                                                            .insert(qubit);
                                                    // Directed graph
                                                    di_qubit_connections
                                                        [prev_qubit]
                                                            .insert(qubit);
                                                }
                                                prev_qubit = qubit;

                                                // Count 2-qubit gates
                                                if (first_qubit == "collected")
                                                {
                                                    continue;
                                                }
                                                else if (first_qubit != "")
                                                {
                                                    two_qubit_gate_counts
                                                        [first_qubit]++;
                                                    two_qubit_gate_counts
                                                        [qubit]++;
                                                    num_two_qubit_gates++;
                                                    first_qubit = "collected";
                                                }
                                                first_qubit = qubit;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        });

    // Calculate the circuit depth (and associated qubit)
    // Count non-idle qubits (see activity matrix)
    int depth = 0, activity_count = 0;
    std::string max_depth_qubit;
    for (const auto &pair : qubit_counts)
    {
        if (pair.second > depth)
        {
            depth = pair.second;
            max_depth_qubit = pair.first;
        }
        activity_count += pair.second;
    }

    // Number of active qubits the circuit
    int num_qubits = qubit_counts.size();

    // Calculate the (directed_)programm_communication
    int degree_sum = 0, degree_sum_di = 0;
    for (const auto &pair : qubit_connections)
    {
        degree_sum += pair.second.size();
    }
    for (const auto &pair : di_qubit_connections)
    {
        degree_sum_di += pair.second.size();
    }
    double program_communication =
        num_qubits > 1 ? (double)degree_sum / (num_qubits * (num_qubits - 1))
                       : 0;
    double directed_program_communication =
        num_qubits > 1 ? (double)degree_sum_di / (num_qubits * (num_qubits - 1))
                       : 0;

    // Calculate critical depth
    double critical_depth =
        num_two_qubit_gates > 0
            ? (double)two_qubit_gate_counts[max_depth_qubit] /
                  (double)num_two_qubit_gates
            : 0.0;

    // Calculate entanglement ratio
    double entanglement_ratio =
        num_gates > 0 ? (double)num_two_qubit_gates / (double)num_gates : 0.0;

    // Calculate average number of gates per layer
    double one_qubit_gates_per_layer = // non_two_qubit_gates_per_layer
        (num_qubits > 0 && depth > 0)
            ? (double)(num_gates - num_two_qubit_gates) /
                  (double)(depth * num_qubits)
            : 0.0;
    double two_qubit_gates_per_layer =
        (num_qubits > 0 && depth > 0)
            ? (double)num_two_qubit_gates / (double)(depth * (num_qubits / 2))
            : 0.0;

    // Calculate parallelism
    double parallelism = (num_qubits > 1 && depth > 0)
                             ? (((double)num_gates / (double)depth) - 1) *
                                   (1 / (double)(num_qubits - 1))
                             : 0.0;

    // Calculate liveness
    double liveness =
        (num_qubits > 1 && depth > 0)
            ? (double)activity_count / (double)(depth * num_qubits)
            : 0.0;

    return std::vector<double>{program_communication,
                               critical_depth,
                               entanglement_ratio,
                               parallelism,
                               liveness,
                               directed_program_communication,
                               one_qubit_gates_per_layer,
                               two_qubit_gates_per_layer};
}
