/* Routine for evaluating population members  */

#include <cmath>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "PassRunner.hpp"

#include "nsga2.hpp"
#include "predictor.hpp"
#include "rand.hpp"

#include <llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

using llvm::orc::ThreadSafeContext;
using llvm::orc::ThreadSafeModule;

/* Routine to evaluate objective function values and constraints for a
 * population */
void evaluate_pop(NSGA2Type *nsga2Params, population *pop,
                  ThreadSafeModule &TSM,
                  const std::vector<std::string> designSpace, bool fVerbose)
{
    int i;
    std::cout << "AAA\n";
    for (i = 0; i < nsga2Params->popsize; i++)
        evaluate_ind(nsga2Params, &(pop->ind[i]), TSM, designSpace, fVerbose);
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

/* Routine to evaluate objective function values and constraints for an
 * individual */
void evaluate_ind(NSGA2Type *nsga2Params, individual *ind,
                  ThreadSafeModule &TSM,
                  const std::vector<std::string> designSpace, bool fVerbose)
{
    // Apply passes here
    std::vector<std::string>
        passes; //(nsga2Params->nint) /*(*(nsga2Params->max_intvar))*/;

    int i, j;

    std::cout << "EVAL 1\n";
    for (j = 0; j < nsga2Params->nint; j++)
    {
        if (ind->xint[j] == -1)
        {
            std::cout << "-1 FOUND!!!!!\n";
            continue;
        }
        passes.push_back(designSpace[ind->xint[j]]);
    }
    std::cout << "EVAL 2\n";

    std::unique_ptr<Module> copiedModule =
        CloneModule(*TSM.getModuleUnlocked());
    ThreadSafeModule copiedTSM(std::move(copiedModule), TSM.getContext());
    invokeTargetSpecificPasses(copiedTSM, passes,
                               nsga2Params->device /*, fVerbose*/);

    if (fVerbose)
    {
        std::remove("/home/ubuntu/logs/optimised_circuit.ll");
        std::error_code EC;
        raw_fd_ostream File("/home/ubuntu/logs/optimised_circuit.ll", EC,
                            sys::fs::OF_Text);
        copiedTSM.getModuleUnlocked()->print(File, nullptr, false, false);
    }
    else
    {

        ind->obj[0] = evaluate_gates(copiedTSM);
        ind->obj[1] = evaluate_depth(copiedTSM);
        ind->obj[2] = evaluate_entanglement_ratio(copiedTSM);
        ind->obj[3] = evaluate_critical_depth(copiedTSM);
        ind->obj[4] = evaluate_parallelism(copiedTSM);

        std::vector<std::string> ga_models = {
            "q20_ga_number_of_gates",    "q20_ga_depth",
            "q20_ga_entanglement_ratio", "q20_ga_critical_depth",
            "q20_ga_parallelism",
        };

        std::map<std::string, float> score;

        score = predict(TSM, ga_models);

        if (ind->obj[0] <= round(score[ga_models[0]]) &&
            ind->obj[1] <= round(score[ga_models[1]]) &&
            ind->obj[2] <= score[ga_models[2]] &&
            ind->obj[3] <= score[ga_models[3]] &&
            ind->obj[4] <= score[ga_models[4]])

        {
            nsga2Params->cont_search = false;
        }

        ind->constr_violation = 0.0;

        if (nsga2Params->ncon != 0)
            for (j = 0; j < nsga2Params->ncon; j++)
                if (ind->constr[j] < 0.0)
                    ind->constr_violation += ind->constr[j];
    }
}
