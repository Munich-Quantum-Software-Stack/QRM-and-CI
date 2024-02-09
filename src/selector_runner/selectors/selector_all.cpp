/**
 * @file selector_all.cpp
 * @brief Implementation of a dummy selector.
 */

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
            if (entry.is_regular_file() && entry.path().extension() == ".so")
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

    std::string passes = passesEnv;
    std::vector<std::string> soPaths = separatePaths(passes);
    std::vector<std::string> soPasses = findSOPasses(soPaths);

    // PERFORM PASS SELECTION

    std::cout << "   [Selector]............Returning list of passes to the "
                 "Selector Runner"
              << std::endl;

    return soPasses;
}
