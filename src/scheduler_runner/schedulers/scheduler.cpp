/**
 * @file scheduler.cpp
 * @brief Implementation of a ML guided scheduler.
 */
#include "QuantumResourceManager.hpp"
#include "eval.hpp"
#include "predictor.hpp"
#include <cstddef>
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

    scores = predict(task.thread_safe_module, {"q20"}); // ONLY FOR TEST

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
        // Predict expected fidelity for every device
        scores = predict(task.thread_safe_module, task.preferred_qpus);
    }

    std::cout << "   [Scheduler]...........Scores: ";
    for (auto &score : scores)
    {
        std::cout << score.first << " " << score.second << " ";
    }
    std::cout << " for task ID " << task.task_id << std::endl;
    return scores;
}

/**
 * @brief Select the shortest queue among the top 3 scored devices.
 * @param scores The scores of the devices.
 * @return The selected target device.
 */
std::string choose_device(const std::map<std::string, float> &scores)
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
        float end_time = queue->tasks.back()->end;
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
bool skipping_schedule(QuantumTask *new_task, std::string &target_device)
{
    // Get current queue from metadata
    QirPassRunner &QPR = QirPassRunner::getInstance();
    QirMetadata &qirMetadata = QPR.getMetadata();
    auto queue = qirMetadata.get_queue(target_device);

    // Age increment after already queued task was skipped by new_task
    // e.g. value of 1/2: integer priority level will increase after 2 skips
    float age_increment = 0.5;

    // Check if the queue is empty
    if (queue->tasks.empty())
    {
        queue->insertTask(0, new_task, new_task->duration);
        new_task->end = new_task->duration;
        std::cout
            << "   [Scheduler]...........Inserting (Child)QuantumTask with ID "
            << new_task->task_id << " into the " << target_device
            << " queue at position 0" << std::endl;
    }
    else
    {
        int i = 0;
        QuantumTask *last_task = NULL;

        for (i = queue->tasks.size(); i > 0; --i)
        {
            last_task = queue->tasks[i - 1];
            float predicted_end = last_task->end + new_task->duration;

            // always skip lower priority tasks
            if (new_task->priority >
                std::floor(last_task->priority + last_task->age))
            {
                // update the end time of the (to be) skipped task
                last_task->end = predicted_end;
                // increase age of the (to be) skipped task
                last_task->age = last_task->age + age_increment;
                continue; // check next job in line
            }
            // possibly skip tasks with same priority
            else if (new_task->priority == last_task->priority)
            {
                // we dont have access to the parent task directly
                int last_parent_id = (last_task->parent_id == -1)
                                         ? last_task->task_id
                                         : last_task->parent_id;
                // so we keep track of their end times in metadata
                float last_parent_end =
                    qirMetadata.get_parent_end(last_parent_id);

                // can skip in line (wo delaying other task)
                if (predicted_end < last_parent_end)
                {
                    // we dont have access to the parent task directly
                    float new_parent_id = (new_task->parent_id == -1)
                                              ? new_task->task_id
                                              : new_task->parent_id;
                    // so we keep track of their end times in metadata
                    float new_parent_end =
                        qirMetadata.get_parent_end(new_parent_id);

                    // should skip in line (for overall speedup)
                    if (new_parent_end < last_parent_end)
                    {
                        // update the end time of the (to be) skipped task
                        last_task->end = predicted_end;
                        // increase age of the (to be) skipped task
                        last_task->age = last_task->age + age_increment;
                        continue; // check next job in line
                    }
                }
            }
            break; // no (more) skipping
        }
        // insert new_task at position i
        queue->insertTask(i, new_task, new_task->duration);
        new_task->end =
            (i == 0 ? new_task->duration
                    : queue->tasks[i - 1]->end + new_task->duration);

        std::cout
            << "   [Scheduler]...........Inserting (Child)QuantumTask with ID "
            << new_task->task_id << " into the " << target_device
            << " queue at position " << i << std::endl;
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

    // Sort tasks (by priority and within that) by duration
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
        std::string target_device = choose_device(scores);

        // Predict the expected execution time for the task on the chosen device
        std::map<std::string, float> duration_prediction =
            predict(task.thread_safe_module, {target_device});
        // TODO: once we have a trained model, use it for duration prediction
        // task.duration = duration_prediction[target_device];
        task.duration =
            calculate_circuit_duration(task.thread_safe_module, 0.04, 0.6, 15);

        // Queue the task on the chosen device and skip if possible
        bool success = skipping_schedule(&task, target_device);

        // TODO once FOMAC is available, set the QPU
        // task.scheduled_qpu = target_device;
    }
    return 0;
}
