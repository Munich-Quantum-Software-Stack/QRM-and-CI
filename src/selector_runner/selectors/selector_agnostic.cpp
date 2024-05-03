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
        // B
        passesPath + "/libQirBarrierBeforeFinalMeasurementsPass.so",
        // C
        passesPath + "/libQirCNotToHCZHDecompositionPass.so",
        passesPath + "/libQirCommuteCnotRxPass.so",
        passesPath + "/libQirCommuteRxCnotPass.so",
        passesPath + "/libQirCommuteCnotXPass.so",
        passesPath + "/libQirCommuteXCnotPass.so",
        passesPath + "/libQirCommuteCnotZPass.so",
        passesPath + "/libQirCommuteZCnotPass.so",
        passesPath + "/libQirCZToHCnotHDecompositionPass.so",
        // D
        passesPath + "/libQirDeferMeasurementPass.so",
        // F
        passesPath + "/libQirFunctionAnnotatorPass.so",
        passesPath + "/libQirFunctionReplacementPass.so",
        // G
        passesPath + "/libQirGroupingPass.so",
        // H
        passesPath + "/libQirHadamardAndXGateSwitchPass.so",
        passesPath + "/libQirHadamardAndYGateSwitchPass.so",
        passesPath + "/libQirHadamardAndZGateSwitchPass.so",
        // M
        passesPath + "/libQirMergeRotationsPass.so",
        // N
        passesPath + "/libQirNormalizeArgAnglePass.so",
        passesPath + "/libQirNullRotationCancellationPass.so",
        // P
        passesPath + "/libQirPlaceIrreversibleGatesInMetadataPass.so",
        passesPath + "/libQirPauliGateAndHadamardSwitchPass.so",
        // Q
        passesPath + "/libQirQubitRemapPass.so",
        // R
        passesPath + "/libQirRedundantGatesCancellationPass.so",
        passesPath + "/libQirRemoveBasicBlocksWithSingleNonConditionalBranchInstsPass.so",
        passesPath + "/libQirReplaceConstantBranchesPass.so",
        passesPath + "/libQirResourceAnnotationPass.so",
        passesPath + "/libQirReverseCnotPass.so",
        passesPath + "/libQirRzToRxRyRxDecompositionPass.so",
        // S
        passesPath + "/libQirSDaggerToSPass.so",
        passesPath + "/libQirSToSDaggerPass.so",
        passesPath + "/libQirSwapAndCnotReplacementPass.so",
        passesPath + "/libQirSwapToCnotsDecompositionPass.so",
        // X
        passesPath + "/libQirXCnotXReductionPass.so",
        passesPath + "/libQirXGateAndHadamardSwitchPass.so",
        passesPath + "/libQirXYXDecompositionPass.so",
        // Y
        passesPath + "/libQirYGateAndHadamardSwitchPass.so",
        // Z
        passesPath + "/libQirZGateAndHadamardSwitchPass.so",
        passesPath + "/libQirZXZDecompositionPass.so",
        passesPath + "/libQirZYZDecompositionPass.so",
    };

    std::cout << "   [Selector]............Returning list of passes to the "
                 "Selector Runner"
              << std::endl;

    std::reverse(passes.begin(), passes.end());
    return passes;
}
