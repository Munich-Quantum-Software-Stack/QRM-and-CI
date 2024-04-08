/**
 * @file selector_ga.cpp
 * @brief Implementation of a ga-based selector.
 */

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <qdmi.h>

#include "nsga2.hpp"

using llvm::orc::ThreadSafeModule;

/**
 * @brief TODO
 * @param TODO
 */
std::vector<std::string> separatePaths(const std::string &passesPaths)
{
    std::vector<std::string> paths;
    std::istringstream ss(passesPaths);
    std::string path;

    while (std::getline(ss, path, ':'))
        if (path != "")
            paths.push_back(path);

    return paths;
}

/**
 * @brief TODO
 * @param TODO
 */
std::vector<std::string> findSOPasses(const std::vector<std::string> &paths)
{
    std::vector<std::string> soPasses;

    for (const std::string &path : paths)
        for (const auto &entry : std::filesystem::directory_iterator(path))
            if (entry.is_regular_file() && entry.path().extension() == ".dylib")
            {
                std::string filename = entry.path().filename().string();
                if (filename.substr(0, 6) == "libQir" &&
                    filename.find("Analysis") == std::string::npos)
                    soPasses.push_back(entry.path().string());
            }

    return soPasses;
}

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

    //std::string passesPath = passesEnv;

    //if (passesPath.find(':') != std::string::npos)
    //{
    //    std::cout << "   [Selector]............Warning: Environment variable "
    //                 "$PASSES contains more than one path"
    //              << std::endl;

    //    return std::vector<std::string>{};
    //}

	std::string pathPasses = passesEnv;
    std::vector<std::string> soPaths = separatePaths(pathPasses);
    std::vector<std::string> passes = findSOPasses(soPaths);

    // Perform the DSE
    std::cout << "   [Selector]............Starting the DSE" << std::endl;
    NSGA2Type nsga2Params = ReadParameters(passes.size(), 2, device);
    std::cout << "   [Selector]............nsga2Params set" << std::endl;
    char *home = std::getenv("HOME");
    assert(home != nullptr);
    std::string results_path = std::string(home) + "/qrm.git/logs/results_";
    const char *GA_path = results_path.c_str();
    std::cout << "   [Selector]............Init NSGA2" << std::endl;
    InitNSGA2(&nsga2Params, TSM, GA_path, false, passes);
    std::cout << "   [Selector]............NSGA2" << std::endl;
    return NSGA2(&nsga2Params, TSM, false, passes);
}
