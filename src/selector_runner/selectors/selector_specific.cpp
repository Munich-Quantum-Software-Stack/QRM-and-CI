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
        //passesPath + "/libQirQMapPass.so",
        passesPath + "/libQirAnnotateUnsupportedGatesPass.so",
    };

    std::cout << "   [Selector]............Returning list of passes to the "
                 "Selector Runner"
              << std::endl;

    std::reverse(passes.begin(), passes.end());
    return passes;
}
