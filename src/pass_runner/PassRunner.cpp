/**
 * @file PassRunner.cpp
 * @brief TODO
 */

#include "PassRunner.hpp"

using llvm::orc::ThreadSafeModule;

/**
 * @brief TODO
 * @param TODO
 */
void invokePasses(ThreadSafeModule &TSM, const std::vector<std::string> &passes)
{
    TSM.withModuleDo(
        [&](Module &module)
        {
            // Attach metadata to the IR
            Metadata *metadata = ConstantAsMetadata::get(
                ConstantInt::get(module.getContext(), APInt(1, true)));

            module.addModuleFlag(Module::Warning, "lrz_supports_qir", metadata);
            module.setSourceFileName("");

            Metadata *metadataSupport =
                module.getModuleFlag("lrz_supports_qir");
            if (metadataSupport)
                if (ConstantAsMetadata *boolMetadata =
                        dyn_cast<ConstantAsMetadata>(metadataSupport))
                    if (ConstantInt *boolConstant =
                            dyn_cast<ConstantInt>(boolMetadata->getValue()))
                        errs() << "   [Pass Runner].........Flag inserted: "
                                  "\"lrz_supports_qir\" = "
                               << (boolConstant->isOne() ? "true" : "false")
                               << '\n';

            // Create an instance of the QirPassRunner and append to it all the
            // received passes
            QirPassRunner &QPR = QirPassRunner::getInstance();
            ModuleAnalysisManager MAM;

            for (std::string libPass : passes)
                QPR.append(libPass);

            // Run QIR passes
            QPR.run(module, MAM);

            // Free memory
            QPR.clearMetadata();
        });
}

/**
 * @brief TODO
 * @param TODO
 */
void invokePasses(std::unique_ptr<Module> &module,
                  const std::vector<std::string> &passes)
{
    if (!module)
    {
        std::cout << "   [Pass Runner].........Warning: Corrupt QIR module "
                  << std::endl;
        return;
    }

    if (passes.empty())
    {
        std::cout << "   [Pass Runner].........Warning: Not passes found"
                  << std::endl;
        return;
    }

    // Attach metadata to the IR
    Metadata *metadata = ConstantAsMetadata::get(
        ConstantInt::get(module->getContext(), APInt(1, true)));

    module->addModuleFlag(Module::Warning, "lrz_supports_qir", metadata);
    module->setSourceFileName("");

    Metadata *metadataSupport = module->getModuleFlag("lrz_supports_qir");
    if (metadataSupport)
        if (ConstantAsMetadata *boolMetadata =
                dyn_cast<ConstantAsMetadata>(metadataSupport))
            if (ConstantInt *boolConstant =
                    dyn_cast<ConstantInt>(boolMetadata->getValue()))
                errs() << "   [Pass Runner].........Flag inserted: "
                          "\"lrz_supports_qir\" = "
                       << (boolConstant->isOne() ? "true" : "false") << '\n';

    // Create an instance of the QirPassRunner and append to it all the
    // received passes
    QirPassRunner &QPR = QirPassRunner::getInstance();
    ModuleAnalysisManager MAM;

    for (std::string libPass : passes)
        QPR.append(libPass);

    // Run QIR passes
    QPR.run(*module, MAM);

    // Free memory
    QPR.clearMetadata();
}
