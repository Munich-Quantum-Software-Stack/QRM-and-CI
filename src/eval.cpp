/* Routine for evaluating population members  */

#include <iostream>
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
