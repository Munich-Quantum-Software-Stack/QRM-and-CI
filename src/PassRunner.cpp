/**
 * @file PassRunner.cpp
 * @brief TODO
 */

#include "PassRunner.hpp"
#include <PassModule.hpp>
#include <string>
#include <dlfcn.h>

using namespace llvm;
using llvm::orc::ThreadSafeModule;

/**
 * @brief Apply target-agnostic passes
 * @param TODO
 */
void invokePasses(ThreadSafeModule &TSM,
                  const std::vector<std::string> &passes)
{
    TSM.withModuleDo(
        [&](Module &module)
        {
            //// Attach metadata to the IR
            //Metadata *metadata = ConstantAsMetadata::get(
            //    ConstantInt::get(module.getContext(), APInt(1, true)));

            //module.addModuleFlag(Module::Warning, "lrz_supports_qir", metadata);
            //module.setSourceFileName("");

            //Metadata *metadataSupport =
            //    module.getModuleFlag("lrz_supports_qir");
            //if (metadataSupport)
            //    if (ConstantAsMetadata *boolMetadata =
            //            dyn_cast<ConstantAsMetadata>(metadataSupport))
            //        if (ConstantInt *boolConstant =
            //                dyn_cast<ConstantInt>(boolMetadata->getValue()))
            //            errs() << "   [Pass Runner].........Flag inserted: "
            //                      "\"lrz_supports_qir\" = "
            //                   << (boolConstant->isOne() ? "true" : "false")
            //                   << '\n';

            // Create an instance of the QirPassRunner and append to it all the
            // received passes
            //QirPassRunner &QPR = QirPassRunner::getInstance();
            ModuleAnalysisManager MAM;

            //for (std::string libPass : passes)
            //    QPR.appendAgnostic(libPass);

            // Run QIR passes
            //QPR.run(module, MAM);

            // Free memory
            //QPR.clearMetadata();
            using passLoader = AgnosticPassModule *(*)();
            for(std::string pass : passes){

                void *lib_handle = dlopen(pass.c_str(), RTLD_LAZY);
                passLoader loadQirPass =
                    reinterpret_cast<passLoader>(dlsym(lib_handle, "loadQirPass"));
                if(!loadQirPass){
                    continue;
                }
                AgnosticPassModule *QirPass = loadQirPass();
                QirPass->run(module, MAM);

            }



        });
}

/**
 * @brief Apply target-specific passes
 * @param TODO
 */
void invokePasses(ThreadSafeModule &TSM,
                  const std::vector<std::string> &passes,
                  QDMI_Device device)
{
    TSM.withModuleDo(
        [&](Module &module)
        {
            //// Attach metadata to the IR
            //Metadata *metadata = ConstantAsMetadata::get(
            //    ConstantInt::get(module.getContext(), APInt(1, true)));

            //module.addModuleFlag(Module::Warning, "lrz_supports_qir", metadata);
            //module.setSourceFileName("");

            //Metadata *metadataSupport =
            //    module.getModuleFlag("lrz_supports_qir");
            //if (metadataSupport)
            //    if (ConstantAsMetadata *boolMetadata =
            //            dyn_cast<ConstantAsMetadata>(metadataSupport))
            //        if (ConstantInt *boolConstant =
            //            dyn_cast<ConstantInt>(boolMetadata->getValue()))
            //            errs() << "   [Pass Runner].........Flag inserted: "
            //                      "\"lrz_supports_qir\" = "
            //                   << (boolConstant->isOne() ? "true" : "false")
            //                   << '\n';

            // Create an instance of the QirPassRunner and append to it all the
            // received passes
            /*
            QirPassRunner &QPR = QirPassRunner::getInstance();
            ModuleAnalysisManager MAM;

            for (std::string libPass : passes)
                QPR.appendSpecific(libPass);

            // Run QIR passes
            QPR.run(module, MAM, device);

            // Free memory
            QPR.clearMetadata();

            */
        });
}
