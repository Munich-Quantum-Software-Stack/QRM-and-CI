/**
 * @file scheduler_heuristic.cpp
 * @brief Implementation of a dummy scheduler.
 */

#include <iostream>
#include <string>
#include <vector>

#include "PassModule.hpp"

#include "../../my_qdmi.hpp"
#include <fomac.hpp>
#include <qdmi.h>

using llvm::orc::ThreadSafeModule;

void heuristic(QuantumTask &task, const std::map<std::string, float> &scores) {

  // Find the platforms with the three highest final scores
  std::vector<std::string> platforms;
  for (auto &score : scores) {
    if (platforms.size() < 3) {
      platforms.push_back(score.first);
    } else {
      for (auto &platform : platforms) {
        if (score.second > scores.at(platform)) {
          platform = score.first;
          break;
        }
      }
    }
  }

  // Get current queue from metadata
  QirPassRunner &QPR = QirPassRunner::getInstance();
  QirMetadata &qirMetadata = QPR.getMetadata();

  // Find the queue with shortest end time among the three platforms
  auto &queues = qirMetadata.queues;
  std::string target_platform;
  float min_end_time = std::numeric_limits<float>::max();

  for (auto &platform : platforms) {
    auto it = std::find_if(
        queues.begin(), queues.end(),
        [&platform](const Queue &queue) { return queue.platform == platform; });

    if (it != queues.end()) {
      if (it->end_time == nullptr || *(it->end_time) < min_end_time) {
        target_platform = platform;
        min_end_time = it->end_time != nullptr ? *(it->end_time) : min_end_time;
      }
    }
  }

  auto &queue = *std::find_if(queues.begin(), queues.end(),
                              [&target_platform](const Queue &queue) {
                                return queue.platform == target_platform;
                              });

  // use skipping routine
  if (queue.tasks.empty()) {
    queue.insertTask(0, &task);
    task.updateEndTime(duration);
  } else {
    for (int i = queue.tasks.size() - 1; i >= 0; --i) {
      QuantumTask &old_task = *queue.tasks[i];
      QuantumTask &old_parent = old_task.parent ? *old_task.parent : old_task;
      float predicted_end = old_task.end_time + task.duration;
      float old_parent_end = old_parent.end_time;
      if (predicted_end < old_parent_end) {
        // can skip in line (wo delaying other task)
        QuantumTask &new_parent = task.parent ? *task.parent : task;
        if (new_parent.end_time < old_parent_end) {
          // should skip in line (for overall speedup)
          if (i != 0) { // have not reached the end of the line
            old_task.updateEndTime(predicted_end);
            continue; // check next job in line
          }
        }
      }

      queue.insertTask(queue.tasks.size() - i, &task);
      task.end_time = task.duration;
      break;
    }
  }
}

/**
 * @brief The main entry point of the program.
 *
 * The Scheduler.
 *
 * @return const char *
 */
extern "C" void scheduler(QuantumTask &task) {
  std::cout << "   [Scheduler]..............Invoking the heuristic scheduler"
            << std::endl;

  // Query the available devices
  std::vector<QDMI_Device> devices = FOMAC_available_devices();

  std::map<std::string, float> scores;
  // Check if the user only wants to use a single QPU
  if (task.preferred_qpus.size() == 1) {
    // Check if the QPU is available
    if (std::find(devices.begin(), devices.end(), task.preferred_qpus[0]) ==
        devices.end()) {
      std::cout << "   [Scheduler]...........Error: The preferred QPU is not "
                   "available"
                << std::endl;
      return;
    }
    scores = {{task.preferred_qpus[0], 1.0}};
  } else { // If choice is not forced, use the recommender system
    // TODO: Calculate ML scores
    std::vector<float> model_scores(devices.size());
    for (auto &score : model_scores) {
      score = static_cast<float>(rand()) / static_cast<float>(123);
    }

    // Map the model output to a std::map<std::string, float>
    for (size_t i = 0; i < devices.size(); ++i) {
      scores[devices[i]] = model_scores[i];
    }
  }

  // TODO: scheduling strategy
  heuristic(task, scores);
  return;
}
