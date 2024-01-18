/* Routine for evaluating population members  */

#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../pass_runner/PassRunner.hpp"

#include "nsga2.hpp"
#include "rand.hpp"

#include <llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>

using llvm::orc::ThreadSafeContext;
using llvm::orc::ThreadSafeModule;

/* Routine to evaluate objective function values and constraints for a
 * population */
void evaluate_pop(NSGA2Type *nsga2Params, population *pop,
                  ThreadSafeModule &TSM,
                  const std::vector<std::string> designSpace, bool fVerbose)
{
    int i;
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

/* Routine to evaluate objective function values and constraints for an
 * individual */
void evaluate_ind(NSGA2Type *nsga2Params, individual *ind,
                  ThreadSafeModule &TSM,
                  const std::vector<std::string> designSpace, bool fVerbose)
{
    std::cout << "[DEBUG] START EVALUATE IND\n";
    // Apply passes here
    std::vector<std::string>
        passes; //(nsga2Params->nint) /*(*(nsga2Params->max_intvar))*/;

    int i, j;

    for (j = 0; j < nsga2Params->nint; j++)
    {
        std::cout << "[DEBUG] PASSES: " << designSpace[ind->xint[j]]
                  << std::endl;
        passes.push_back(designSpace[ind->xint[j]]);
    }

    std::cout << "[DEBUG] after loop\n";
    invokePasses(TSM, passes /*, fVerbose*/);
    std::cout << "[DEBUG] after invoke\n";

    ind->obj[0] = evaluate_gates(TSM);
    std::cout << "[DEBUG] after gates\n";
    ind->obj[1] = evaluate_depth(TSM);
    std::cout << "[DEBUG] after depth\n";

    ind->constr_violation = 0.0;

    if (nsga2Params->ncon != 0)
        for (j = 0; j < nsga2Params->ncon; j++)
            if (ind->constr[j] < 0.0)
                ind->constr_violation += ind->constr[j];
    std::cout << "[DEBUG] end\n";
}
