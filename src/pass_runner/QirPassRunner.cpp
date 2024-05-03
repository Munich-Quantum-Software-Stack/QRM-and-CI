/**
 * @file QirPassRunner.cpp
 * @brief Implementation of the 'QirPassRunner' class.
 */

#include "QirPassRunner.hpp"
#include <algorithm>
#include <iostream>
#include <string>

using namespace llvm;

/**
 * @brief Default constructor of the 'QirPassRunner' class
 */
QirPassRunner::QirPassRunner() {}

/**
 * @brief Returns a reference to a 'QirPassRuner' object
 * @return QirPassRunner
 */
QirPassRunner &QirPassRunner::getInstance()
{
    static QirPassRunner instance;
    return instance;
}

/**
 * @brief Saves 'metadata' as the private metadata of the
 * 'QirPassRunner' class.
 * @param metadata The metadata attached to the module.
 */
void QirPassRunner::setMetadata(const QirMetadata &metadata)
{
    qirMetadata_ = metadata;
}

/**
 * @brief Empties all structures within the metadata.
 */
void QirPassRunner::clearMetadata()
{
    qirMetadata_.reversibleGates.clear();
    qirMetadata_.supportedGates.clear();
    qirMetadata_.availablePlatforms.clear();
    qirMetadata_.injectedAnnotations.clear();
}

/**
 * @brief Returns the private metadata of the 'QirPassRunner' class
 * @return QirMetadata
 */
QirMetadata &QirPassRunner::getMetadata() { return qirMetadata_; }

/**
 *  Inserts a pass in the private vector 'specificPasses_' of the
 * 'QirPassRunner' class
 */
void QirPassRunner::appendSpecific(std::string pass) { specificPasses_.push_back(pass); }

/**
 *  Inserts a pass in the private vector 'agnosticPasses_' of the
 * 'QirPassRunner' class
 */
void QirPassRunner::appendAgnostic(std::string pass) { agnosticPasses_.push_back(pass); }

/**
 * @brief  Applies all target-agnostic passes in the private vector 'agnosticPasses_' to the
 * QIR parsed into an LLVM module 'module'
 * @param module The module of the submitted QIR.
 * @param MAM The module analysis manager.
 */
void /*PreservedAnalyses*/
QirPassRunner::run(Module &module, ModuleAnalysisManager &MAM)
{
    // TODO HOW DO WE HANDLE 'PreservedAnalyses'?
    // PreservedAnalyses PA;

    while (!agnosticPasses_.empty())
    {
        // Get the name of the pass compiled as a shared library
        auto pass = agnosticPasses_.back();

        // Load the library
        void *lib_handle = dlopen(pass.c_str(), RTLD_LAZY);

        if (!lib_handle)
        {
            std::cout << "   [Pass Runner].........Warning: Could not load "
                         "shared library: "
                      << pass << dlerror() << std::endl;

            agnosticPasses_.pop_back();
            continue;
        }

        // Format the name of the pass and print it on screen
        size_t lastSlash = pass.find_last_of('/');
        std::string passName = pass.substr(lastSlash + 4);
        size_t lastDot = passName.find_last_of('.');
        std::string passNameWithoutExt = passName.substr(0, lastDot);

        std::cout << "   [Pass Runner].........Applying target-agnostic pass: "
                  << passNameWithoutExt << std::endl;

        // Pointer to 'loadQirPass' function returning a pointer to the
        // 'AgnosticPassModule' object
        using passLoader = AgnosticPassModule *(*)();

        // Dynamic loading and linking of the shared library
        passLoader loadQirPass =
            reinterpret_cast<passLoader>(dlsym(lib_handle, "loadQirPass"));

        if (!loadQirPass)
        {
            std::cout << "   [Pass Runner].........Warning: Could not get "
                         "factory function "
                         "of pass: "
                      << pass << std::endl;

            agnosticPasses_.pop_back();
            dlclose(lib_handle);
            continue;
        }

        AgnosticPassModule *QirPass = loadQirPass();

        // Apply the pass to the LLVM module 'module'
        /*PA =*/QirPass->run(module, MAM);

        // Free memory
        delete QirPass;
        dlclose(lib_handle);

        agnosticPasses_.pop_back();
    }

    // return PA;
}

/**
 * @brief  Applies all target-specific passes in the private vector 'specificPasses_' to the
 * QIR parsed into an LLVM module 'module'
 * @param module The module of the submitted QIR.
 * @param MAM The module analysis manager.
 * @param dev The QDMI device.
 */
void /*PreservedAnalyses*/
QirPassRunner::run(Module &module, ModuleAnalysisManager &MAM, QDMI_Device dev)
{
    // TODO HOW DO WE HANDLE 'PreservedAnalyses'?
    // PreservedAnalyses PA;

    while (!specificPasses_.empty())
    {
        // Get the name of the pass compiled as a shared library
        auto pass = specificPasses_.back();

        // Load the library
        void *lib_handle = dlopen(pass.c_str(), RTLD_LAZY);

        if (!lib_handle)
        {
            std::cout << "   [Pass Runner].........Warning: Could not load "
                         "shared library: "
                      << pass << dlerror() << std::endl;

            specificPasses_.pop_back();
            continue;
        }

        // Format the name of the pass and print it on screen
        size_t lastSlash = pass.find_last_of('/');
        std::string passName = pass.substr(lastSlash + 4);
        size_t lastDot = passName.find_last_of('.');
        std::string passNameWithoutExt = passName.substr(0, lastDot);

        std::cout << "   [Pass Runner].........Applying target-specific pass: "
                  << passNameWithoutExt << std::endl;

        // Pointer to 'loadQirPass' function returning a pointer to the
        // 'SpecificPassModule' object
        //std::cout << "   [Pass Runner].........1" << std::endl;
        using passLoader = SpecificPassModule *(*)();
        //std::cout << "   [Pass Runner].........2" << std::endl;

        // Dynamic loading and linking of the shared library
        passLoader loadQirPass =
            reinterpret_cast<passLoader>(dlsym(lib_handle, "loadQirPass"));
        //std::cout << "   [Pass Runner].........3" << std::endl;

        if (!loadQirPass)
        {
            std::cout << "   [Pass Runner].........Warning: Could not get "
                         "factory function "
                         "of pass: "
                      << pass << std::endl;

            specificPasses_.pop_back();
            dlclose(lib_handle);
            continue;
        }

        //std::cout << "   [Pass Runner].........4" << std::endl;
        SpecificPassModule *QirPass = loadQirPass();

        //std::cout << "   [Pass Runner].........5" << std::endl;
        // Apply the pass to the LLVM module 'module'
        /*PA =*/QirPass->run(module, MAM, dev);
        //std::cout << "   [Pass Runner].........6" << std::endl;

        // Free memory
        delete QirPass;
        //std::cout << "   [Pass Runner].........7" << std::endl;
        dlclose(lib_handle);
        //std::cout << "   [Pass Runner].........8" << std::endl;

        specificPasses_.pop_back();
        //std::cout << "   [Pass Runner].........9" << std::endl;
    }

    // return PA;
}
