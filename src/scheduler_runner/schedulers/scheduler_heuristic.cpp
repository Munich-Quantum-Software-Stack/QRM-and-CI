/**
 * @file scheduler_heuristic.cpp
 * @brief Implementation of a dummy scheduler.
 */

#include <iostream>
#include <string>
#include <vector>

#include "PassModule.hpp"
#include "QuantumResourceManager.hpp"

#include "../../my_qdmi.hpp"
#include <fomac.hpp>
#include <qdmi.h>

using llvm::orc::ThreadSafeModule;

bool skipping(QuantumTask &new_task, const std::string &target_platform)
{

    // Get current queue from metadata
    QirPassRunner &QPR = QirPassRunner::getInstance();
    QirMetadata &qirMetadata = QPR.getMetadata();

    auto queue = qirMetadata.get_queue(target_platform);
    float new_task_duration = qirMetadata.get_duration(new_task.task_id);
    int new_task_priority = qirMetadata.get_priority(new_task.task_id);

    std::cout << "   [Scheduler]...........Inserting QuantumTask with ID "
              << new_task.task_id << " into the queue for platform "
              << target_platform << std::endl;

    // Check if the queue is empty
    if (queue->tasks.empty())
    {
        queue->insertTask(0, &new_task, new_task_duration);
        qirMetadata.update_end(new_task.task_id, new_task_duration);
    }
    else
    {
        int i = 0;
        for (i = queue->tasks.size() - 1; i >= 0; --i)
        {
            const QuantumTask &last_task = *queue->tasks[i];
            float last_task_end = qirMetadata.get_end(last_task.task_id);
            float last_task_priority =
                qirMetadata.get_priority(last_task.task_id);

            float predicted_end = last_task_end + new_task_duration;

            if (new_task_priority > last_task_priority)
            {
                // always skip lower priority tasks
                qirMetadata.update_end(last_task.task_id, predicted_end);
                qirMetadata.update_priority(last_task.task_id,
                                            last_task_priority + 1);
                continue; // check next job in line
            }
            else if (new_task_priority == last_task_priority)
            {
                int last_parent_id = (last_task.parent_id == -1)
                                         ? last_task.task_id
                                         : last_task.parent_id;
                float last_parent_end = qirMetadata.get_end(last_parent_id);

                if (predicted_end < last_parent_end)
                {
                    // can skip in line (wo delaying other task)
                    float new_parent_id = (new_task.parent_id == -1)
                                              ? new_task.task_id
                                              : new_task.parent_id;
                    float new_parent_end = qirMetadata.get_end(new_parent_id);

                    if (new_parent_end < last_parent_end)
                    {
                        // should skip in line (for overall speedup)
                        qirMetadata.update_end(last_task.task_id,
                                               predicted_end);
                        qirMetadata.update_priority(last_task.task_id,
                                                    last_task_priority + 1);
                        continue; // check next job in line
                    }
                }
            }
            // no (more) skipping
            break;
        }
        // insert new_task at position i
        queue->insertTask(i, &new_task, new_task_duration);
        qirMetadata.update_end(new_task.task_id, new_task_duration);
    }
    return true;
}

std::string choose_platform(QuantumTask &task,
                            const std::map<std::string, float> &scores)
{
    // Find the platforms with the three highest final scores
    std::vector<std::string> platforms;
    for (auto &score : scores)
    {
        if (platforms.size() < 3)
        {
            platforms.push_back(score.first);
        }
        else
        {
            for (auto &platform : platforms)
            {
                if (score.second > scores.at(platform))
                {
                    platform = score.first;
                    break;
                }
            }
        }
    }

    std::cout << "   [Scheduler]...........Choosing target QDMI_Device from"
              << " the following platforms: ";
    for (auto &platform : platforms)
    {
        std::cout << platform << " ";
    }

    // Get current queue from metadata
    QirPassRunner &QPR = QirPassRunner::getInstance();
    QirMetadata &qirMetadata = QPR.getMetadata();

    // Find the queue with shortest end time among the three platforms
    float min_end_time = std::numeric_limits<float>::max();
    std::string target_platform;

    for (auto &platform : platforms)
    {
        auto queue = qirMetadata.get_queue(platform);
        if (queue->tasks.empty())
        {
            target_platform = platform;
            break;
        }
        float end_time = qirMetadata.get_end(queue->tasks.back()->task_id);
        if (end_time < min_end_time)
        {
            min_end_time = end_time;
            target_platform = platform;
        }
    }
    return target_platform;
}

/**
 * @brief The main entry point of the program.
 *
 * The Scheduler.
 *
 * @return const char *
 */
extern "C" QDMI_Device scheduler(std::vector<QuantumTask> &tasks)
{
    // TODO uncomment when FOMAC is available
    // std::vector<QDMI_Device> devices = FOMAC_available_devices();

    std::vector<std::string> devices = {"Q5", "Q20", "Q50"};

    std::cout << "   [Scheduler]..........." << devices.size()
              << " available device(s)" << std::endl;

    std::cout << "   [Scheduler]...........preffered QPU: ";

    // Sort tasks by priority and within that by duration
    std::sort(tasks.begin(), tasks.end(),
              [](const QuantumTask &a, const QuantumTask &b)
              {
                  if (a.priority == b.priority)
                  {
                      return a.duration > b.duration;
                  }
                  return a.priority > b.priority;
              });

    for (auto &task : tasks)
    {

        for (auto &qpu : task.preferred_qpus)
        {
            std::cout << qpu << " ";
        }

        std::map<std::string, float> scores;
        // Check if the user only wants to use a single QPU
        if (task.preferred_qpus.size() == 1)
        {
            // TODO: Check if the QPU is available

            // Maximal score for the preferred QPU
            scores = {{task.preferred_qpus[0], 1.0}};
        }
        else
        { // If choice is not forced, use the recommender system
            // TODO: Calculate ML scores

            // Dummy scores. TODO: use devices when available
            std::vector<std::string> qpus = task.preferred_qpus;
            std::vector<float> model_scores(qpus.size());
            for (auto &score : model_scores)
            {
                score =
                    static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            }
            // Map the model output to a std::map<std::string, float>
            for (size_t i = 0; i < qpus.size(); ++i)
            {
                scores[qpus[i]] = model_scores[i];
            }
        }

        std::cout << "   [Scheduler]...........Scores: ";
        for (auto &score : scores)
        {
            std::cout << score.first << " " << score.second << " ";
        }

        // Choose the platform with the shortest queue out of top 3
        std::string target_platform = choose_platform(task, scores);

        // Queue the task on the chosen platform and skip if necessary
        bool success = skipping(task, target_platform);
    }

    std::cout << "   [Scheduler]...........returning selected device."
              << std::endl;

    return QDMI_Device();
}
