/**
 * @file generator_cutter.cpp
 * @brief Implementation of a dummy generator.
 */

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "llvm.hpp"
#include <QuantumResourceManager.hpp>
#include <qdmi.h>
#include <qinfo.h>

using namespace llvm;
using llvm::orc::ThreadSafeContext;
using llvm::orc::ThreadSafeModule;

#define CHECK_ERR(a, b)                                                        \
    {                                                                          \
        if (a != QDMI_SUCCESS)                                                 \
        {                                                                      \
            std::cout << std::endl << "[Error]: " << a << " at " << b;         \
        }                                                                      \
    }

/**
 * @brief TODO
 * @return QuantumTask
 */
QuantumTask createQuantumTask(std::unique_ptr<Module> &module,
                              ThreadSafeContext TSCtx,
                              const QuantumTask &parentQuantumTask,
                              const int task_id)
{
    ThreadSafeModule TSM =
        ThreadSafeModule(std::move(module), std::move(TSCtx));

    QuantumTask childQuantumTask;

    childQuantumTask.parent_id = parentQuantumTask.task_id;
    childQuantumTask.task_id = task_id;
    childQuantumTask.n_qbits = parentQuantumTask.n_qbits;
    childQuantumTask.n_shots = parentQuantumTask.n_shots;
    childQuantumTask.circuit_file = parentQuantumTask.circuit_file;
    childQuantumTask.circuit_file_type = parentQuantumTask.circuit_file_type;
    childQuantumTask.preferred_qpu = parentQuantumTask.preferred_qpu;
    childQuantumTask.scheduled_qpu = parentQuantumTask.scheduled_qpu;
    childQuantumTask.priority = parentQuantumTask.priority;
    childQuantumTask.optimisation_level = parentQuantumTask.optimisation_level;
    childQuantumTask.no_modify = parentQuantumTask.no_modify;
    childQuantumTask.transpiler_flag = parentQuantumTask.transpiler_flag;
    childQuantumTask.result_type = parentQuantumTask.result_type;
    childQuantumTask.submit_time = parentQuantumTask.submit_time;
    childQuantumTask.qir = parentQuantumTask.qir;
    childQuantumTask.additional_information =
        parentQuantumTask.additional_information;
    childQuantumTask.thread_safe_module = std::move(TSM);

    return childQuantumTask;
}

/**
 * @brief The main entry point of the program.
 *
 * This generator clones the parent QuantumTask
 * two identical child QuantumTasks
 *
 * @return std::vector<QuantumTask>
 */
extern "C" std::vector<QuantumTask>
generator(const QuantumTask &parentQuantumTask)
{
    std::vector<QuantumTask> childQuantumTasks;

    // Parse generic QIR into an LLVM module
    ThreadSafeContext TSCtx1(std::make_unique<LLVMContext>());
    ThreadSafeContext TSCtx2(std::make_unique<LLVMContext>());

    SMDiagnostic error;
    std::string circuit = parentQuantumTask.qir;

    auto M1 = parseIR(MemoryBufferRef(circuit, "QIR (LRZ)"), error,
                      *TSCtx1.getContext());

    auto M2 = parseIR(MemoryBufferRef(circuit, "QIR (LRZ)"), error,
                      *TSCtx2.getContext());

    childQuantumTasks.push_back(
        createQuantumTask(M1, TSCtx1, parentQuantumTask, 0));

    childQuantumTasks.push_back(
        createQuantumTask(M2, TSCtx2, parentQuantumTask, 1));

    std::cout << "   [Generator]...........Returning generated "
              << "sub-circuits to the Generator Runner" << std::endl;

    return childQuantumTasks;
}
