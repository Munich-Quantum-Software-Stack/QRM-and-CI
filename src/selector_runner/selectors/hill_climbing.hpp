#ifndef NSGA2_HPP
#define NSGA2_HPP

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "rand.hpp"
#include "metrics.hpp"
#include "PassRunner.hpp"

using llvm::orc::ThreadSafeModule;

void logResults(std::vector<double> scores, 
    std::vector<std::vector<int>> solutions);

double evaluateSolution(std::vector<int> currentSolution, 
    const std::vector<std::string> designSpace, 
    ThreadSafeModule &TSM, QDMI_Device &device);

std::vector<int> generateRandomSolution(int length);

std::vector<int> steepDescentHillClimbing(int maxIterations, double &result, const std::vector<std::string> designSpace, 
    ThreadSafeModule &TSM, QDMI_Device &device);

std::vector<std::string> executeHillClimbing(
    const std::vector<std::string> designSpace,
    ThreadSafeModule &TSM,
    QDMI_Device &device);

#endif
