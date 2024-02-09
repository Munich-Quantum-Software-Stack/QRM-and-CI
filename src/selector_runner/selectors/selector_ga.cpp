/**
 * @file selector_ga.cpp
 * @brief Implementation of a dummy ga-based selector.
 */

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "nsga2.hpp"

using llvm::orc::ThreadSafeModule;

/**
 * @brief The main entry point of the program.
 *
 * The Selector.
 *
 * @return std::vector<std::string>
 */
extern "C" std::vector<std::string> selector(ThreadSafeModule &TSM)
{
    char *passesEnv = std::getenv("PASSES");

    if (passesEnv == nullptr)
    {
        std::cout << "   [Selector]............Warning: Environment variable "
                     "$PASSES not found"
                  << std::endl;

        return std::vector<std::string>{};
    }

    std::string passesPath = passesEnv;

    if (passesPath.find(':') != std::string::npos)
    {
        std::cout << "   [Selector]............Warning: Environment variable "
                     "$PASSES contains more than one path"
                  << std::endl;

        return std::vector<std::string>{};
    }

    // Append the existing passes
    std::vector<std::string> passes{
        passesPath + "/libQirDivisionByZeroPass.so",
        passesPath + "/libQirNormalizeArgAnglePass.so",
        // passesPath + "/libQirXCnotXReductionPass.so", TODO BUG?
        passesPath + "/libQirCommuteCnotRxPass.so",
        passesPath + "/libQirCommuteRxCnotPass.so",
        passesPath + "/libQirCommuteCnotXPass.so",
        passesPath + "/libQirCommuteXCnotPass.so",
        passesPath + "/libQirCommuteCnotZPass.so",
        passesPath + "/libQirCommuteZCnotPass.so",
        passesPath + "/libQirPlaceIrreversibleGatesInMetadataPass.so",
        passesPath + "/libQirAnnotateUnsupportedGatesPass.so",
        passesPath + "/libQirU3ToRzRyRzDecompositionPass.so",
        passesPath + "/libQirRzToRxRyRxDecompositionPass.so",
        passesPath + "/libQirCNotToHCZHDecompositionPass.so",
        passesPath + "/libQirSwapToCnotsDecompositionPass.so",
        passesPath + "/libQirCZToHCnotHDecompositionPass.so",
        passesPath + "/libQirFunctionAnnotatorPass.so",
        passesPath + "/libQirRedundantGatesCancellationPass.so",
        passesPath + "/libQirFunctionReplacementPass.so",
        passesPath + "/libQirReplaceConstantBranchesPass.so",
        passesPath + "/libQirGroupingPass.so",
        passesPath + "/libQirRemoveNonEntrypointFunctionsPass.so",
        passesPath + "/libQirDeferMeasurementPass.so",
        passesPath + "/libQirBarrierBeforeFinalMeasurementsPass.so",
        // passesPath +
        //  "/libQirRemoveBasicBlocksWithSingleNonConditionalBranchIns"
        //               "tsPass.so",
        //   passesPath + "/libQirQubitRemapPass.so", TODO BREAKS THE DEPTH'S
        //   FITNESS EVALUATION
        passesPath + "/libQirResourceAnnotationPass.so",
        passesPath + "/libQirNullRotationCancellationPass.so",
        passesPath + "/libQirMergeRotationsPass.so",
        passesPath + "/libQirDoubleCnotCancellationPass.so",
        passesPath + "/libQirHadamardAndXGateSwitchPass.so",
        passesPath + "/libQirHadamardAndYGateSwitchPass.so",
        passesPath + "/libQirHadamardAndZGateSwitchPass.so",
        passesPath + "/libQirXGateAndHadamardSwitchPass.so",
        passesPath + "/libQirYGateAndHadamardSwitchPass.so",
        passesPath + "/libQirZGateAndHadamardSwitchPass.so",
        passesPath + "/libQirSToSDaggerPass.so",
        passesPath + "/libQirSDaggerToSPass.so",
        passesPath + "/libQirReverseCnotPass.so",
        passesPath + "/libQirSwapAndCnotReplacementPass.so",
    };

    // Perform the DSE
    std::cout << "   [Selector]............Starting the DSE" << std::endl;
    NSGA2Type nsga2Params = ReadParameters(passes.size(), 2);
    std::cout << "   [Selector]............nsga2Params set" << std::endl;
    char *home = std::getenv("HOME");
    assert(home != nullptr);
    std::string results_path = std::string(home) + "/logs/results_";
    const char *GA_path = results_path.c_str();
    std::cout << "   [Selector]............Init NSGA2" << std::endl;
    InitNSGA2(&nsga2Params, TSM, GA_path, false, passes);
    std::cout << "   [Selector]............NSGA2" << std::endl;
    NSGA2(&nsga2Params, TSM, false, passes);
    std::cout << "   [Selector]............Finished the DSE" << std::endl;

    // Return the list of chosen passes
    return passes;
}
