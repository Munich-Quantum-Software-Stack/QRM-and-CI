/**
 * @file scheduler_heuristic.cpp
 * @brief Implementation of a dummy scheduler.
 */

#include <iostream>
#include <string>
#include <vector>

#include "PassModule.hpp"

#include <fomac.hpp>
#include <qdmi.h>
#include "../../my_qdmi.hpp"

using llvm::orc::ThreadSafeModule;

void heuristic(const int &priority, const std::map<std::string, float> &scores,
               Job &job)
{
    // Find the platform with the highest final score
    std::string target_platform =
        std::max_element(scores.begin(), scores.end(),
                         [](const auto &a, const auto &b)
                         { return a.second < b.second; })
            ->first;

    // Get current metadata
    QirPassRunner &QPR = QirPassRunner::getInstance();
    QirMetadata &qirMetadata = QPR.getMetadata();

    // Get the queue and task end times from metadata
    auto &queue = qirMetadata.queues[target_platform];
    auto &task_end_times = qirMetadata.task_end_times;

    // Iterate over each job in the queue
    auto it = queue.begin();
    for (it; it != queue.end(); ++it)
    {

        // Calculate how the final end time of the job's associated task would
        // be affected by the new job
        float affected_end_time =
            task_end_times[it->task_id] + job.execution_time;

        // If the affected end time is not later than the current end time of
        // the new job's task, insert the new job here
        if (affected_end_time <= task_end_times[job.task_id])
        {
            queue.insert(it, job);
            break;
        }
    }

    // Update the end time of the new job's task
    task_end_times[job.task_id] = it->execution_time + job.execution_time;

    // If no suitable position was found, add the new job to the end of the
    // queue
    if (it == queue.end())
    {
        queue.push_back(job);
    }

    // Set the target platform
    qirMetadata.setTargetPlatform(target_platform);
    QPR.setMetadata(qirMetadata);
}

/**
 * @brief The main entry point of the program.
 *
 * The Scheduler.
 *
 * @return const char *
 */
extern "C" void scheduler(const ThreadSafeModule &TSM, const int &priority,
                          const std::map<std::string, float> &preferred_qpu,
                          Job &job)
{
    std::cout << "   [Scheduler]..............Invoking the heuristic scheduler"
              << std::endl;

    // Query the available platforms
    std::vector<std::string> platforms = FOMAC_available_devices();

    // Check if the user only wants to use a single QPU
    std::map<std::string, float> scores = preferred_qpu;
    bool must_use_specific_device = false;
    for (const auto &pair : scores)
    {
        if (pair.second == 1.0)
        {
            must_use_specific_device = true;
            break;
        }
    }

    // If choice is not forced, use the recommender system
    if (must_use_specific_device == false)
    {

        // TODO: Calculate ML scores
        std::vector<float> model_scores(platforms.size());
        for (auto &score : model_scores)
        {
            score = static_cast<float>(rand()) / static_cast<float>(123);
        }

        // Map the model output to a std::map<std::string, float>
        std::map<std::string, float> model_scores_map;
        for (size_t i = 0; i < platforms.size(); ++i)
        {
            model_scores_map[platforms[i]] = model_scores[i];
        }

        // Check that the number of model scores matches the number of user
        // preferences
        if (model_scores_map.size() != preferred_qpu.size())
        {
            std::cout << "   [Scheduler]...........Error: The number of model "
                         "scores does not match the number of user preferences"
                      << std::endl;
            return;
        }

        for (const std::string &platform : platforms)
        {
            if (preferred_qpu.find(platform) != preferred_qpu.end())
            {
                scores[platform] =
                    preferred_qpu.at(platform) * model_scores_map.at(platform);
            }
        }
    }

    // TODO: scheduling strategy
    heuristic(priority, scores, job);
}
