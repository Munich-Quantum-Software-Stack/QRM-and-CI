/**
 * @file scheduler.cpp
 * @brief Implementation of a ML guided scheduler.
 */
#include "QuantumResourceManager.hpp"
#include "predictor.hpp"
#include <fomac.hpp>
#include <iostream>
#include <qdmi.h>
#include <string>
#include <vector>

using llvm::orc::ThreadSafeModule;
/**
 * @brief Calculate scores for the devices. Either based on user preference, ML
 * model, or both.
 * @param task The QuantumTask to be scheduled.
 * @return Scores for the devices.
 */
std::map<std::string, float> calculate_scores(QuantumTask &task)
{
    std::map<std::string, float> scores;

    // User only wants to use a single QPU
    if (task.preferred_qpus.size() == 1)
    {
        // maximum score for the only QPU
        scores = {{task.preferred_qpus.front(), 1.0}};
    }
    // TODO
    // else if (user wants to use some QPUs more than others):
    //      scores = task.preferred_qpus
    else
    { // If choice is not forced or the user preference is equally distributed
        for (auto &device : task.preferred_qpus)
        { // Predict expected fidelity for every device
            scores[device] = predict(task.thread_safe_module, device);
        }
    }

    std::cout << "   [Scheduler]...........Scores: ";
    for (auto &score : scores)
    {
        std::cout << score.first << " " << score.second << " ";
    }
    return scores;
}

/**
 * @brief Select the shortest queue among the top 3 scored devices.
 * @param task The QuantumTask to be scheduled.
 * @param scores The scores of the devices.
 * @return The name of the selected device.
 */
std::string choose_device(QuantumTask &task,
                          const std::map<std::string, float> &scores)
{
    // Find the devices with the three highest final scores
    std::vector<std::string> devices;
    for (auto &score : scores)
    {
        if (devices.size() < 3)
        {
            devices.push_back(score.first);
        }
        else
        {
            for (auto &device : devices)
            {
                if (score.second > scores.at(device))
                {
                    device = score.first;
                    break;
                }
            }
        }
    }

    std::cout << "   [Scheduler]...........Choosing target QDMI_Device from"
              << " the following devices: ";
    for (auto &device : devices)
    {
        std::cout << device << " ";
    }

    // Get current queue from metadata
    QirPassRunner &QPR = QirPassRunner::getInstance();
    QirMetadata &qirMetadata = QPR.getMetadata();

    // Find the queue with shortest end time among the three devices
    float min_end_time = std::numeric_limits<float>::max();
    std::string target_device;

    for (auto &device : devices)
    {
        auto queue = qirMetadata.get_queue(device);
        if (queue->tasks.empty())
        {
            target_device = device;
            break;
        }
        float end_time = qirMetadata.get_end(queue->tasks.back()->task_id);
        if (end_time < min_end_time)
        {
            min_end_time = end_time;
            target_device = device;
        }
    }
    return target_device;
}

/**
 * @brief Schedule a QuantumTask on a target device using skipping strategy.
 * @param new_task The QuantumTask to be scheduled.
 * @param target_device The target device to schedule the QuantumTask on.
 * @return True if the QuantumTask was successfully scheduled, false otherwise.
 */
bool skipping_schedule(QuantumTask &new_task, std::string &target_device)
{
    // Get current queue from metadata
    QirPassRunner &QPR = QirPassRunner::getInstance();
    QirMetadata &qirMetadata = QPR.getMetadata();

    auto queue = qirMetadata.get_queue(target_device);
    float new_task_duration = qirMetadata.get_duration(new_task.task_id);
    int new_task_priority = qirMetadata.get_priority(new_task.task_id);

    // Age increment after already queued task was skipped by new_task
    // e.g. value of 1/2: integer priority level will increase after 2 skips
    float age_increment = 0.5;

    std::cout << "   [Scheduler]...........Inserting QuantumTask with ID "
              << new_task.task_id << " into the queue for device "
              << target_device << std::endl;

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
            QuantumTask &last_task = *queue->tasks[i];
            float last_task_end = qirMetadata.get_end(last_task.task_id);
            float last_task_priority =
                qirMetadata.get_priority(last_task.task_id);

            float predicted_end = last_task_end + new_task_duration;

            if (new_task_priority >
                std::floor(last_task_priority + last_task.age))
            {
                // always skip lower priority tasks
                qirMetadata.update_end(last_task.task_id, predicted_end);
                // increase age of skipped task
                last_task.age = last_task.age + age_increment;
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
                        // increase age of skipped task
                        last_task.age = last_task.age + age_increment;
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

/**
 * @brief Entry point for the scheduler.
 * @param task The QuantumTask to be scheduled.
 * @return The selected device on which the task was scheduled.
 */
extern "C" int scheduler(std::vector<QuantumTask> *tasks)
{
    // TODO uncomment when FOMAC is available
    // std::vector<QDMI_Device> devices = FOMAC_available_devices();
    std::vector<std::string> devices = {"Q5", "Q20", "Q50"};

    std::cout << "   [Scheduler]..........." << devices.size()
              << " available device(s)" << std::endl;

    // Sort tasks by priority and within that by duration
    std::sort((*tasks).begin(), (*tasks).end(),
              [](const QuantumTask &a, const QuantumTask &b)
              {
                  if (a.priority == b.priority)
                  {
                      return a.duration > b.duration;
                  }
                  return a.priority > b.priority;
              });

    // Queue each task
    for (auto &task : *tasks)
    {
        // Calculate scores to produce device ranking
        std::map<std::string, float> scores = calculate_scores(task);

        // Choose the device with the shortest queue out of top 3
        std::string target_device = choose_device(task, scores);

        // Queue the task on the chosen device and skip if necessary
        bool success = skipping_schedule(task, target_device);

        // TODO once FOMAC is available, set the QPU
        // task.scheduled_qpu = target_device;
    }

    std::cout << "   [Scheduler]...........returning selected device."
              << std::endl;

    return 0;
}
