/**
 * @file selector_manual.cpp
 * @brief Implementation of a dummy selector.
 */

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief The main entry point of the program.
 *
 * The Selector.
 *
 * @return std::vector<std::string>
 */
extern "C" std::vector<std::string> selector(void)
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

    // Append the desired passes
    std::vector<std::string> passes{
        //passesPath + "/libQirDivisionByZeroPass.so",
        ////passesPath + "/libQirNormalizeArgAnglePass.so",
        //passesPath + "/libQirXCnotXReductionPass.so",
        //passesPath + "/libQirCommuteCnotRxPass.so",
        //passesPath + "/libQirCommuteRxCnotPass.so",
        //passesPath + "/libQirCommuteCnotXPass.so",
        //passesPath + "/libQirCommuteXCnotPass.so",
        //passesPath + "/libQirCommuteCnotZPass.so",
        //passesPath + "/libQirCommuteZCnotPass.so",
        //passesPath + "/libQirPlaceIrreversibleGatesInMetadataPass.so",
        //// passesPath + "/libQirU3ToRzRyRzDecompositionPass.so",
        //// passesPath + "/libQirRzToRxRyRxDecompositionPass.so", TODO Bug?
        //passesPath + "/libQirCNotToHCZHDecompositionPass.so",
        //passesPath + "/libQirSwapToCnotsDecompositionPass.so",
        //passesPath + "/libQirCZToHCnotHDecompositionPass.so",
        //// passesPath + "/libQirU3DecompositionPass.so",
        //// passesPath + "/libQirXYXDecompositionPass.so",
        //// passesPath + "/libQirZXZDecompositionPass.so",
        //// passesPath + "/libQirZYZDecompositionPass.so",
        //passesPath + "/libQirFunctionAnnotatorPass.so",
        //passesPath + "/libQirRedundantGatesCancellationPass.so",
        //passesPath + "/libQirGroupingPass.so",
        //passesPath + "/libQirDeferMeasurementPass.so",
        ////passesPath + "/libQirBarrierBeforeFinalMeasurementsPass.so",
        //passesPath + "/libQirRemoveBasicBlocksWithSingleNonConditionalBranchIns"
        //             "tsPass.so",
        //passesPath + "/libQirQubitRemapPass.so",
        //passesPath + "/libQirResourceAnnotationPass.so",
        //// passesPath + "/libQirNullRotationCancellationPass.so",
        //// passesPath + "/libQirMergeRotationsPass.so",
        //// passesPath + "/libQirDoubleCnotCancellationPass.so",
        //passesPath + "/libQirHadamardAndXGateSwitchPass.so",
        //passesPath + "/libQirHadamardAndYGateSwitchPass.so",
        //passesPath + "/libQirHadamardAndZGateSwitchPass.so",
        //passesPath + "/libQirXGateAndHadamardSwitchPass.so",
        //passesPath + "/libQirYGateAndHadamardSwitchPass.so",
        //passesPath + "/libQirZGateAndHadamardSwitchPass.so",
        //passesPath + "/libQirSToSDaggerPass.so",
        //passesPath + "/libQirSDaggerToSPass.so",
        //passesPath + "/libQirReverseCnotPass.so",
        //passesPath + "/libQirSwapAndCnotReplacementPass.so",
        //passesPath + "/libQirFunctionReplacementPass.so",
        passesPath + "/libQirReplaceConstantBranchesPass.so",
        //passesPath + "/libQirRemoveNonEntrypointFunctionsPass.so",
    };

    std::cout << "   [Selector]............Returning list of passes to the "
                 "Selector Runner"
              << std::endl;

    std::reverse(passes.begin(), passes.end());
    return passes;
}
