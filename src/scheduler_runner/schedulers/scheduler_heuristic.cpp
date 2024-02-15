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
        if (score.second > scores[platform]) {
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
  for (auto &platform : platforms) {
    for (auto &queue : queues) {
      if (queue.platform == platform) {
        if (queue.end_time == nullptr) {
          target_platform = platform;
          break;
        }
        if (queue.end_time < queues[target_platform].end_time) {
          target_platform = platform;
        }
      }
    }
  }

  auto &queue = queues[target_platform];
  // use skipping routine
  if (queue.tasks.empty()) {
    queue.addTask(&task);
    task.end_time = task.duration;
  } else {
    for (int i = queue.tasks.size() - 1; i >= 0; --i) {
      QuantumTask &old_task = *queue.tasks[i];
      float predicted_end = old_task.end_time + task.duration;
      float task_end = old_task.task.end_time;
      if (predicted_end <
          task_end) { // can skip in line (wo delaying other task)
        if (new_task.end_time <
            task_end) { // should skip in line (for overall speedup)
          if (i != 0) { // have not reached the end of the line
            old_task.end_time =
                predicted_end; // update end time of current job in line
            continue;          // check next job in line
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
  } else { // If choice is not forced, use the recommender system
    // TODO: Calculate ML scores
    std::vector<float> model_scores(devices.size());
    for (auto &score : model_scores) {
      score = static_cast<float>(rand()) / static_cast<float>(123);
    }

    // Map the model output to a std::map<std::string, float>
    std::map<std::string, float> model_scores_map;
    for (size_t i = 0; i < devices.size(); ++i) {
      model_scores_map[devices[i]] = model_scores[i];
    }
  }

  // TODO: scheduling strategy
  heuristic(task, scores);
  return;
}
