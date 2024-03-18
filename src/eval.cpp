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

std::map<std::string, int> evaluate_gate_counts(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";
    std::map<std::string, int> gate_counts = {
        {"u3", 0},      {"u2", 0},    {"u1", 0},   {"cx", 0},   {"id", 0},
        {"u0", 0},      {"u", 0},     {"p", 0},    {"x", 0},    {"y", 0},
        {"z", 0},       {"h", 0},     {"s", 0},    {"sdg", 0},  {"t", 0},
        {"tdg", 0},     {"rx", 0},    {"ry", 0},   {"rz", 0},   {"sx", 0},
        {"sxdg", 0},    {"cz", 0},    {"cy", 0},   {"swap", 0}, {"ch", 0},
        {"ccx", 0},     {"cswap", 0}, {"crx", 0},  {"cry", 0},  {"crz", 0},
        {"cu1", 0},     {"cp", 0},    {"cu3", 0},  {"csx", 0},  {"cu", 0},
        {"rxx", 0},     {"rzz", 0},   {"rccx", 0}, {"rc3x", 0}, {"c3x", 0},
        {"c3sqrtx", 0}, {"c4x", 0}};
    int number_of_gates = 0;

    if (!TSM)
    {
        std::cerr << "ThreadSafeModule is null" << std::endl;
        return gate_counts;
    }
    TSM.withModuleDo(
        [&](Module &module)
        {
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
                                auto name =
                                    static_cast<std::string>(f->getName());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                    number_of_gates++;
                            }
                        }
                    }
                }
            }
        });

    return gate_counts;
}

int evaluate_depth(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";
    std::unordered_map<std::string, int> qubit_count;

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
                        if (auto *call_instr = dyn_cast<CallBase>(&instruction))
                        {
                            if (auto *f = call_instr->getCalledFunction())
                            {
                                auto name = static_cast<std::string>(
                                    f->getName().str());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
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

                                                qubit_count[qubit]++;
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

    int max_depth = 0;
    for (const auto &pair : qubit_count)
        if (pair.second > max_depth)
            max_depth = pair.second;
    return max_depth;
}

int evaluate_gates(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";

    int number_of_gates = 0;

    TSM.withModuleDo(
        [&](Module &module)
        {
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
                                auto name =
                                    static_cast<std::string>(f->getName());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                    number_of_gates++;
                            }
                        }
                    }
                }
            }
        });

    return number_of_gates;
}

/*
 *   Source: "SupermarQ: A Scalable Quantum Benchmark Suite"
 *   entaglement_ratio = multi_qubit_gates / all_gates
 */
double evaluate_entanglement_ratio(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";

    int number_of_gates = 0;
    int number_of_multi_qubit_gates = 0;

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
                                auto name =
                                    static_cast<std::string>(f->getName());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
                                    number_of_gates++;
                                    bool is_second_qubit = false;

                                    for (Use &operand : call_instr->operands())
                                    {
                                        if (auto *val =
                                                dyn_cast<Value>(&operand))
                                        {
                                            if (val->getType() ==
                                                PointerType::get(qubitType, 0))
                                            {
                                                if (is_second_qubit)
                                                {
                                                    number_of_multi_qubit_gates++;
                                                    break;
                                                }
                                                is_second_qubit = true;
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

    if (number_of_gates == 0)
        return 0.0;

    double entanglement_ratio =
        (double)number_of_multi_qubit_gates / (double)number_of_gates;
    return entanglement_ratio;
}

/*
 *   Source: "SupermarQ: A Scalable Quantum Benchmark Suite"
 *   parallelism = ((numer_of_gates / depth) - 1) * (1 / (numer_of_qubits - 1))
 */
double evaluate_parallelism(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";
    std::unordered_map<std::string, int> qubit_count;
    int number_of_gates = 0;

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
                        if (auto *call_instr = dyn_cast<CallBase>(&instruction))
                        {
                            if (auto *f = call_instr->getCalledFunction())
                            {
                                auto name = static_cast<std::string>(
                                    f->getName().str());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
                                    number_of_gates++;

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

                                                qubit_count[qubit]++;
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

    int max_depth = 0;
    for (const auto &pair : qubit_count)
        if (pair.second > max_depth)
            max_depth = pair.second;

    if (max_depth == 0 || qubit_count.size() == 1)
        return 0.0;

    double parallelism = (((double)number_of_gates / (double)max_depth) - 1) *
                         (1 / ((double)qubit_count.size() - 1));

    return parallelism;
}

/*
 *   Source: "SupermarQ: A Scalable Quantum Benchmark Suite"
 *   critical_depth = multi_qubits_gates_on_depth_path / multi_qubits_gates
 */
double evaluate_critical_depth(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";
    std::unordered_map<std::string, int> qubit_count;
    std::unordered_map<std::string, int> multi_qubit_gates;
    int all_multi_qubit_gates = 0;

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
                        if (auto *call_instr = dyn_cast<CallBase>(&instruction))
                        {
                            if (auto *f = call_instr->getCalledFunction())
                            {
                                auto name = static_cast<std::string>(
                                    f->getName().str());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
                                    std::string prev_qubit = "";

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
                                                qubit_count[qubit]++;

                                                if (prev_qubit != "")
                                                {
                                                    multi_qubit_gates[qubit]++;
                                                    multi_qubit_gates
                                                        [prev_qubit]++;
                                                    all_multi_qubit_gates++;
                                                    break;
                                                }
                                                prev_qubit = qubit;
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

    if (all_multi_qubit_gates == 0)
        return 0.0;

    int max_depth = 0;
    std::string qubit;

    for (const auto &pair : qubit_count)
    {
        if (pair.second > max_depth)
        {
            max_depth = pair.second;
            qubit = pair.first;
        }
    }

    double critical_depth =
        (double)multi_qubit_gates[qubit] / (double)all_multi_qubit_gates;

    return critical_depth;
}

/*
 *   Source: "SupermarQ: A Scalable Quantum Benchmark Suite"
 *   circuit's average directed qubit degree / degree of a complete directed
 * graph
 */
std::pair<double, double> evaluate_gates_per_layer(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";
    int single_qubit_gates = 0;
    int multi_qubit_gates = 0;
    int depth = 0;
    std::unordered_map<std::string, int> qubit_count;

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
                        if (auto *call_instr = dyn_cast<CallBase>(&instruction))
                        {
                            if (auto *f = call_instr->getCalledFunction())
                            {
                                auto name = static_cast<std::string>(
                                    f->getName().str());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
                                    int qubit_count_in_instruction = 0;

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
                                                qubit_count[qubit]++;
                                                qubit_count_in_instruction++;
                                            }
                                        }
                                    }

                                    if (qubit_count_in_instruction == 1)
                                    {
                                        single_qubit_gates++;
                                    }
                                    else if (qubit_count_in_instruction > 1)
                                    {
                                        multi_qubit_gates++;
                                    }
                                }
                            }
                        }
                    }

                    depth = std::max(depth, (int)block.size());
                }
            }
        });

    int num_qubits = qubit_count.size();
    double singleQ_gates_per_layer =
        num_qubits > 0 ? (double)single_qubit_gates / (depth * num_qubits) : 0;
    double multiQ_gates_per_layer =
        num_qubits > 1 ? (double)multi_qubit_gates / (depth * (num_qubits / 2))
                       : 0;

    return std::make_pair(singleQ_gates_per_layer, multiQ_gates_per_layer);
}

/*
 *   Source: directed program communication
 *   circuit's average directed qubit degree / degree of a complete directed
 * graph
 */
struct Graph
{
    std::map<int, std::set<int>> adjacency_list;

    void add_edge(int u, int v) { adjacency_list[u].insert(v); }

    int degree(int v) { return adjacency_list[v].size(); }
};

std::pair<double, double> evaluate_program_communication(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";
    std::unordered_map<std::string, int> qubit_count;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        qubit_connections;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        di_qubit_connections;

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
                        if (auto *call_instr = dyn_cast<CallBase>(&instruction))
                        {
                            if (auto *f = call_instr->getCalledFunction())
                            {
                                auto name = static_cast<std::string>(
                                    f->getName().str());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
                                    std::string prev_qubit = "";

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
                                                qubit_count[qubit]++;

                                                if (prev_qubit != "")
                                                {
                                                    qubit_connections[qubit]
                                                        .insert(prev_qubit);
                                                    qubit_connections
                                                        [prev_qubit]
                                                            .insert(qubit);
                                                    di_qubit_connections
                                                        [prev_qubit]
                                                            .insert(qubit);
                                                }
                                                prev_qubit = qubit;
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

    int num_qubits = qubit_count.size();

    // Calculate the degree sum
    int degree_sum = 0;
    for (const auto &pair : qubit_connections)
    {
        degree_sum += pair.second.size();
    }

    // Calculate the degree sum for the directed graph
    int degree_sum_di = 0;
    for (const auto &pair : di_qubit_connections)
    {
        degree_sum_di += pair.second.size();
    }

    // Calculate the directed and undirected program communication
    double program_communication =
        num_qubits > 1 ? (double)degree_sum / (num_qubits * (num_qubits - 1))
                       : 0;
    double directed_program_communication =
        num_qubits > 1 ? (double)degree_sum_di / (num_qubits * (num_qubits - 1))
                       : 0;

    return std::make_pair(directed_program_communication,
                          program_communication);
}

int evaluate_liveness(ThreadSafeModule &TSM)
{
    std::string QIS_START = "__quantum__qis_";
    std::unordered_map<std::string, int> qubit_count;

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
                        if (auto *call_instr = dyn_cast<CallBase>(&instruction))
                        {
                            if (auto *f = call_instr->getCalledFunction())
                            {
                                auto name = static_cast<std::string>(
                                    f->getName().str());

                                bool is_quantum =
                                    (name.size() >= QIS_START.size() &&
                                     name.substr(0, QIS_START.size()) ==
                                         QIS_START);

                                if (is_quantum)
                                {
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

                                                qubit_count[qubit]++;
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

    int max_depth = 0;
    for (const auto &pair : qubit_count)
        if (pair.second > max_depth)
            max_depth = pair.second;
    return max_depth;
}
