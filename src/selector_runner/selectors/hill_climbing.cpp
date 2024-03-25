#include "hill_climbing.hpp"

void logResults(std::vector<double> scores, 
    std::vector<std::vector<int>> solutions) 
{
    int i, j;
    FILE *fpt;
    char *home = std::getenv("HOME");
    assert(home != nullptr);
    std::string results_path = std::string(home) + "/logs/results.out";
    const char *path = results_path.c_str();
    
    fpt = fopen(path, "w"); 

    fprintf(fpt, 
        "# This file contains the best results of hill climbing algorithm for: depth, gates, parallelism, critical depth and entanglement ratio\n");

    fprintf(fpt, 
    "# metric_score # solution\n");

    for (i = 0; i < solutions.size(); i++) 
    {
        fprintf(fpt, "\n%e", scores[i]);
        for (j = 0; j < solutions[i].size(); j++)
        {
            fprintf(fpt, "\t%d", solutions[i][j]);
        }
    }

    

}

double evaluateSolution(std::vector<int> currentSolution, 
    const std::vector<std::string> designSpace, 
    ThreadSafeModule &TSM, QDMI_Device &device)
{
    std::vector<std::string> passes;
    
    int i;

    for (i = 0; i < currentSolution.size(); i++)
    { 
        if (currentSolution[i])
            passes.push_back(designSpace[i]);
    }

    invokeTargetSpecificPasses(TSM, passes, device);
    double depth = evaluate_depth(TSM);
    double number_of_gates = evaluate_gates(TSM);
    double critical_depth = evaluate_critical_depth(TSM);
    double entaglement_ratio = evaluate_entanglement_ratio(TSM);
    double parallelism = evaluate_parallelism(TSM);

    double single_metric = depth * number_of_gates * critical_depth * entaglement_ratio * parallelism;
    return single_metric;
}

std::vector<int> generateRandomSolution(int length) {
    std::vector<int> solution;
    for (int i = 0; i < length; ++i) {
        solution.push_back(rnd(0, 1));
    }
    return solution;
}

std::vector<int> steepDescentHillClimbing(int maxIterations, double &result, const std::vector<std::string> designSpace, 
    ThreadSafeModule &TSM, QDMI_Device &device) 
{
    int length = designSpace.size();
    std::vector<int> currentSolution = generateRandomSolution(length);
   
    double currentScore = evaluateSolution(currentSolution, designSpace, TSM, device);

    for (int iter = 0; iter < maxIterations; ++iter) {
        std::vector<int> nextSolution = currentSolution;
        int nextIndex = rnd(0, length - 1);
        nextSolution[nextIndex] = 1 - nextSolution[nextIndex];
        int nextScore = evaluateSolution(nextSolution, designSpace, TSM, device);

        if (nextScore < currentScore) {
            currentSolution = nextSolution;
            currentScore = nextScore;
        }
    }

    result = currentScore;

    return currentSolution;
}

std::vector<std::string> executeHillClimbing(
    const std::vector<std::string> designSpace,
    ThreadSafeModule &TSM,
    QDMI_Device &device)
{
    srand(time(0));
    int i;
    int algorithmIterations = 100;
    int maxIterations = 50;
    std::vector<double> results(algorithmIterations);
    std::vector<std::vector<int>> solutions(algorithmIterations);

    std::cout << "DEBUG " << designSpace.size() << std::endl;

    for (i = 0; i < algorithmIterations; i++) 
    {
        solutions[i] = steepDescentHillClimbing(maxIterations, results[i], designSpace, TSM, device);
    }

    double bestResult = results[0];
    int idx = 0;

    for (i = 1; i < results.size(); i++)
    {
        if (results[i] < bestResult)
        {
            bestResult = results[i];
            idx = i;
        }
    }

    std::vector<std::string> passes;

    for (i = 0; i < solutions[idx].size(); i++)
    { 
        if (solutions[idx][i])
            passes.push_back(designSpace[i]);
    }

    return passes;
}
