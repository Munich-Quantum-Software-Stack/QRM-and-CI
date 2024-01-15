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
    // Append the existing passes
    std::vector<std::string> passes{
        "libQirDivisionByZeroPass.so",
        "libQirNormalizeArgAnglePass.so",
        //"libQirXCnotXReductionPass.so", TODO BUG?
        "libQirCommuteCnotRxPass.so",
        "libQirCommuteRxCnotPass.so",
        "libQirCommuteCnotXPass.so",
        "libQirCommuteXCnotPass.so",
        "libQirCommuteCnotZPass.so",
        "libQirCommuteZCnotPass.so",
        "libQirPlaceIrreversibleGatesInMetadataPass.so",
        "libQirAnnotateUnsupportedGatesPass.so",
        "libQirU3ToRzRyRzDecompositionPass.so",
        "libQirRzToRxRyRxDecompositionPass.so",
        "libQirCNotToHCZHDecompositionPass.so",
        "libQirSwapToCnotsDecompositionPass.so",
        "libQirCZToHCnotHDecompositionPass.so",
        "libQirFunctionAnnotatorPass.so",
        "libQirRedundantGatesCancellationPass.so",
        "libQirFunctionReplacementPass.so",
        "libQirReplaceConstantBranchesPass.so",
        "libQirGroupingPass.so",
        "libQirRemoveNonEntrypointFunctionsPass.so",
        "libQirDeferMeasurementPass.so",
        "libQirBarrierBeforeFinalMeasurementsPass.so",
        "libQirRemoveBasicBlocksWithSingleNonConditionalBranchInstsPass.so",
        //"libQirQubitRemapPass.so", TODO BREAKS THE DEPTH'S FITNESS EVALUATION
        "libQirResourceAnnotationPass.so",
        "libQirNullRotationCancellationPass.so",
        "libQirMergeRotationsPass.so",
        "libQirDoubleCnotCancellationPass.so",
        "libQirHadamardAndXGateSwitchPass.so",
        "libQirHadamardAndYGateSwitchPass.so",
        "libQirHadamardAndZGateSwitchPass.so",
        "libQirXGateAndHadamardSwitchPass.so",
        "libQirYGateAndHadamardSwitchPass.so",
        "libQirZGateAndHadamardSwitchPass.so",
        "libQirSToSDaggerPass.so",
        "libQirSDaggerToSPass.so",
        "libQirReverseCnotPass.so",
        "libQirSwapAndCnotReplacementPass.so",
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
