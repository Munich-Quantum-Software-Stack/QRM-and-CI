/**
 * @file selector_hillclimb.cpp
 * @brief Implementation of a hill climbing-based selector.
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <qdmi.h>
#include "hill_climbing.hpp"

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Module.h>

using llvm::orc::ThreadSafeModule;

/**
 * @brief The main entry point of the program.
 *
 * The Selector.
 *
 * @return std::vector<std::string>
 */
extern "C" std::vector<std::string> selector(ThreadSafeModule &TSM, 
    QDMI_Device &device)
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
        // passesPath + "/libQirQMapPass.dylib",
        passesPath + "/libQirDivisionByZeroPass.dylib",
        passesPath + "/libQirNormalizeArgAnglePass.dylib",
        passesPath + "/libQirXCnotXReductionPass.dylib",
        passesPath + "/libQirCommuteCnotRxPass.dylib",
        passesPath + "/libQirCommuteRxCnotPass.dylib",
        passesPath + "/libQirCommuteCnotXPass.dylib",
        passesPath + "/libQirCommuteXCnotPass.dylib",
        passesPath + "/libQirCommuteCnotZPass.dylib",
        passesPath + "/libQirCommuteZCnotPass.dylib",
        passesPath + "/libQirAnnotateUnsupportedGatesPass.dylib",
        passesPath + "/libQirPlaceIrreversibleGatesInMetadataPass.dylib",
        passesPath + "/libQirU3ToRzRyRzDecompositionPass.dylib",
        // passesPath + "/libQirRzToRxRyRxDecompositionPass.dylib", TODO Bug?
        passesPath + "/libQirCNotToHCZHDecompositionPass.dylib",
        passesPath + "/libQirSwapToCnotsDecompositionPass.dylib",
        passesPath + "/libQirCZToHCnotHDecompositionPass.dylib",
        passesPath + "/libQirU3DecompositionPass.dylib",
        passesPath + "/libQirXYXDecompositionPass.dylib",
        passesPath + "/libQirZXZDecompositionPass.dylib",
        passesPath + "/libQirZYZDecompositionPass.dylib",
        passesPath + "/libQirFunctionAnnotatorPass.dylib",
        passesPath + "/libQirRedundantGatesCancellationPass.dylib",
        passesPath + "/libQirGroupingPass.dylib",
        passesPath + "/libQirDeferMeasurementPass.dylib",
        //passesPath + "/libQirBarrierBeforeFinalMeasurementsPass.dylib",
        //passesPath + "/libQirRemoveBasicBlocksWithSingleNonConditionalBranchIns"
        //             "tsPass.dylib",
        // passesPath + "/libQirQubitRemapPass.dylib",
        passesPath + "/libQirResourceAnnotationPass.dylib",
        passesPath + "/libQirNullRotationCancellationPass.dylib",
        passesPath + "/libQirMergeRotationsPass.dylib",
        passesPath + "/libQirDoubleCnotCancellationPass.dylib",
        passesPath + "/libQirHadamardAndXGateSwitchPass.dylib",
        passesPath + "/libQirHadamardAndYGateSwitchPass.dylib",
        passesPath + "/libQirHadamardAndZGateSwitchPass.dylib",
        passesPath + "/libQirXGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirYGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirZGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirSToSDaggerPass.dylib",
        passesPath + "/libQirSDaggerToSPass.dylib",
        passesPath + "/libQirReverseCnotPass.dylib",
        passesPath + "/libQirSwapAndCnotReplacementPass.dylib",
        passesPath + "/libQirFunctionReplacementPass.dylib",
        passesPath + "/libQirReplaceConstantBranchesPass.dylib",
        passesPath + "/libQirRemoveNonEntrypointFunctionsPass.dylib",
        /* FROM SELECTOR_GA:
        passesPath + "/libQirDivisionByZeroPass.dylib",
        passesPath + "/libQirNormalizeArgAnglePass.dylib",
        // passesPath + "/libQirXCnotXReductionPass.dylib", TODO BUG?
        passesPath + "/libQirCommuteCnotRxPass.dylib",
        passesPath + "/libQirCommuteRxCnotPass.dylib",
        passesPath + "/libQirCommuteCnotXPass.dylib",
        passesPath + "/libQirCommuteXCnotPass.dylib",
        passesPath + "/libQirCommuteCnotZPass.dylib",
        passesPath + "/libQirCommuteZCnotPass.dylib",
        passesPath + "/libQirPlaceIrreversibleGatesInMetadataPass.dylib",
        passesPath + "/libQirAnnotateUnsupportedGatesPass.dylib",
        passesPath + "/libQirU3ToRzRyRzDecompositionPass.dylib",
        passesPath + "/libQirRzToRxRyRxDecompositionPass.dylib",
        passesPath + "/libQirCNotToHCZHDecompositionPass.dylib",
        passesPath + "/libQirSwapToCnotsDecompositionPass.dylib",
        passesPath + "/libQirCZToHCnotHDecompositionPass.dylib",
        passesPath + "/libQirFunctionAnnotatorPass.dylib",
        passesPath + "/libQirRedundantGatesCancellationPass.dylib",
        passesPath + "/libQirFunctionReplacementPass.dylib",
        passesPath + "/libQirReplaceConstantBranchesPass.dylib",
        passesPath + "/libQirGroupingPass.dylib",
        passesPath + "/libQirRemoveNonEntrypointFunctionsPass.dylib",
        passesPath + "/libQirDeferMeasurementPass.dylib",
        passesPath + "/libQirBarrierBeforeFinalMeasurementsPass.dylib",
        // passesPath +
        //  "/libQirRemoveBasicBlocksWithSingleNonConditionalBranchIns"
        //               "tsPass.dylib",
        //   passesPath + "/libQirQubitRemapPass.dylib", TODO BREAKS THE DEPTH'S
        //   FITNESS EVALUATION
        passesPath + "/libQirResourceAnnotationPass.dylib",
        passesPath + "/libQirNullRotationCancellationPass.dylib",
        passesPath + "/libQirMergeRotationsPass.dylib",
        passesPath + "/libQirDoubleCnotCancellationPass.dylib",
        passesPath + "/libQirHadamardAndXGateSwitchPass.dylib",
        passesPath + "/libQirHadamardAndYGateSwitchPass.dylib",
        passesPath + "/libQirHadamardAndZGateSwitchPass.dylib",
        passesPath + "/libQirXGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirYGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirZGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirSToSDaggerPass.dylib",
        passesPath + "/libQirSDaggerToSPass.dylib",
        passesPath + "/libQirReverseCnotPass.dylib",
        passesPath + "/libQirSwapAndCnotReplacementPass.dylib",
        */
    };

    // Perform the DSE
    std::cout << "   [Selector]............Starting the DSE" << std::endl;
    return executeHillClimbing(passes, TSM, device);
}
