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
 * @brief The main entry point of the program.
 *
 * The Generator.
 *
 * @return std::vector<ThreadSafeModule>
 */
extern "C" std::vector<ThreadSafeModule> generator(const std::string circuit)
{
    std::vector<ThreadSafeModule> sub_circuits;

    // Parse generic QIR into an LLVM module
    ThreadSafeContext TSCtx1(std::make_unique<LLVMContext>());
    ThreadSafeContext TSCtx2(std::make_unique<LLVMContext>());

    SMDiagnostic error;

    auto M1 = parseIR(MemoryBufferRef(circuit, "QIR (LRZ)"), error,
                      *TSCtx1.getContext());

    auto M2 = parseIR(MemoryBufferRef(circuit, "QIR (LRZ)"), error,
                      *TSCtx2.getContext());

    sub_circuits.push_back(ThreadSafeModule(std::move(M1), std::move(TSCtx1)));
    sub_circuits.push_back(ThreadSafeModule(std::move(M2), std::move(TSCtx2)));

    std::cout
        << "   [Generator]...........Returning generated sub-circuits to the "
           "Generator Runner"
        << std::endl;

    return sub_circuits;
}
