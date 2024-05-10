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
        passesPath + "/libQirBarrierBeforeFinalMeasurementsPass.dylib",
        // C
        passesPath + "/libQirCNotToHCZHDecompositionPass.dylib",
        passesPath + "/libQirCommuteCnotRxPass.dylib",
        passesPath + "/libQirCommuteRxCnotPass.dylib",
        passesPath + "/libQirCommuteCnotXPass.dylib",
        passesPath + "/libQirCommuteXCnotPass.dylib",
        passesPath + "/libQirCommuteCnotZPass.dylib",
        passesPath + "/libQirCommuteZCnotPass.dylib",
        passesPath + "/libQirCZToHCnotHDecompositionPass.dylib",
        // D
        passesPath + "/libQirDeferMeasurementPass.dylib",
        // F
        passesPath + "/libQirFunctionAnnotatorPass.dylib",
        passesPath + "/libQirFunctionReplacementPass.dylib",
        // G
        passesPath + "/libQirGroupingPass.dylib",
        // H
        passesPath + "/libQirHadamardAndXGateSwitchPass.dylib",
        passesPath + "/libQirHadamardAndYGateSwitchPass.dylib",
        passesPath + "/libQirHadamardAndZGateSwitchPass.dylib",
        passesPath + "/libQirHXHToZPass.dylib",
        passesPath + "/libQirHZHToXPass.dylib",
        // M
        passesPath + "/libQirMergeRotationsPass.dylib",
        // N
        passesPath + "/libQirNormalizeArgAnglePass.dylib",
        passesPath + "/libQirNullRotationCancellationPass.dylib",
        // P
        passesPath + "/libQirPlaceIrreversibleGatesInMetadataPass.dylib",
        passesPath + "/libQirPauliGateAndHadamardSwitchPass.dylib",
        // Q
        passesPath + "/libQirQubitRemapPass.dylib",
        // R
        passesPath + "/libQirRedundantGatesCancellationPass.dylib",
        passesPath + "/libQirRemoveBasicBlocksWithSingleNonConditionalBranchInstsPass.dylib",
        passesPath + "/libQirReplaceConstantBranchesPass.dylib",
        passesPath + "/libQirResourceAnnotationPass.dylib",
        passesPath + "/libQirReverseCnotPass.dylib",
        passesPath + "/libQirRzToRxRyRxDecompositionPass.dylib",
        // S
        passesPath + "/libQirSDaggerToSPass.dylib",
        passesPath + "/libQirSToSDaggerPass.dylib",
        passesPath + "/libQirSwapAndCnotReplacementPass.dylib",
        passesPath + "/libQirSwapToCnotsDecompositionPass.dylib",
        // X
        passesPath + "/libQirXCnotXReductionPass.dylib",
        passesPath + "/libQirXGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirXYXDecompositionPass.dylib",
        // Y
        passesPath + "/libQirYGateAndHadamardSwitchPass.dylib",
        // Z
        passesPath + "/libQirZGateAndHadamardSwitchPass.dylib",
        passesPath + "/libQirZXZDecompositionPass.dylib",
        passesPath + "/libQirZYZDecompositionPass.dylib",
    };

    std::cout << "   [Selector]............Returning list of passes to the "
                 "Selector Runner"
              << std::endl;

    std::reverse(passes.begin(), passes.end());
    return passes;
}
