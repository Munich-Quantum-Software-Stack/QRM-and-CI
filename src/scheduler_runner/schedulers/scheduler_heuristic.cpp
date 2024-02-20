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

QDMI_Device heuristic(QuantumTask &task, const std::map<std::string, float> &scores) {

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

  std::cout << "   [Scheduler]...........Choosing target QDMI_Device from"
            << " the following platforms: ";
  for (auto &platform : platforms) {
    std::cout << platform << " ";
  }

  // Get current queue from metadata
  QirPassRunner &QPR = QirPassRunner::getInstance();
  QirMetadata &qirMetadata = QPR.getMetadata();

  // Find the queue with shortest end time among the three platforms
  std::string target_platform;
  float min_end_time = std::numeric_limits<float>::max();

  for (auto &platform : platforms) {
    auto queue = qirMetadata.get_queue(platform);
    if (queue->tasks.empty()) {
      target_platform = platform;
      break;
    }
    float end_time = qirMetadata.get_end_time(queue->tasks.back()->task_id);
    if (end_time < min_end_time) {
      min_end_time = end_time;
      target_platform = platform;
    }
  }

  auto queue = qirMetadata.get_queue(target_platform);
  
  float duration = qirMetadata.get_duration(task.task_id);


  std::cout << "   [Scheduler]...........Inserting QuantumTask with ID "
            << task.task_id << " into the queue for platform "
            << target_platform << std::endl;

  // use skipping routine
  if (queue->tasks.empty()) {
    queue->insertTask(0, &task, duration);
    qirMetadata.update_end_time(task.task_id, duration);
  } else {
    for (int i = queue->tasks.size() - 1; i >= 0; --i) {
      const QuantumTask &old_task = *queue->tasks[i];
      float predicted_end = qirMetadata.get_end_time(old_task.task_id) + duration;

      int old_parent_id = (old_task.parent_id == -1) ? old_task.task_id : old_task.parent_id;
      float old_parent_end = qirMetadata.get_end_time(old_parent_id);

      if (predicted_end < old_parent_end) {
        // can skip in line (wo delaying other task)
        float new_parent_id = (task.parent_id == -1) ? task.task_id : task.parent_id;
        float new_parent_end = qirMetadata.get_end_time(new_parent_id);

        if (new_parent_end < old_parent_end) {
          // should skip in line (for overall speedup)
          if (i != 0) { // have not reached the end of the line
            qirMetadata.update_end_time(old_task.task_id, predicted_end);
            continue; // check next job in line
          }
        }
      }
      queue->insertTask(queue->tasks.size() - i, &task, duration);
      qirMetadata.update_end_time(task.task_id, duration);
      break;
    }
  }
  return FOMAC_available_devices().back();
}

/**
 * @brief The main entry point of the program.
 *
 * The Scheduler.
 *
 * @return const char *
 */
extern "C" QDMI_Device scheduler(QuantumTask &task) {

  std::vector<QDMI_Device> devices = FOMAC_available_devices();

  std::cout << "   [Scheduler]..........." << devices.size()
            << " available device(s)" << std::endl;

  std::cout << "   [Scheduler]...........preffered QPU: ";
  for (auto &qpu : task.preferred_qpus) {
    std::cout << qpu << " ";
  }

  std::map<std::string, float> scores;
  // Check if the user only wants to use a single QPU
  if (task.preferred_qpus.size() == 1) {
    // TODO: Check if the QPU is available
    // if (std::find(devices.begin(), devices.end(), task.preferred_qpus[0]) ==
    //    devices.end()) {
    //  std::cout << "   [Scheduler]...........Error: The preferred QPU is not "
    //               "available"
    //            << std::endl;
    //  return;
    //}
    scores = {{task.preferred_qpus[0], 1.0}};
  } else { // If choice is not forced, use the recommender system
    // TODO: Calculate ML scores
    std::vector<std::string> qpus = task.preferred_qpus; // TODO: use devices
    std::vector<float> model_scores(qpus.size());
    for (auto &score : model_scores) {
      score = static_cast<float>(rand()) / static_cast<float>(123);
    }

    // Map the model output to a std::map<std::string, float>
    for (size_t i = 0; i < qpus.size(); ++i) {
      scores[qpus[i]] = model_scores[i];
    }
  }

  std::cout << "   [Scheduler]...........Scores: ";
  for (auto &score : scores) {
    std::cout << score.first << " " << score.second << " ";
  }

  // TODO: scheduling strategy
  QDMI_Device dev = heuristic(task, scores);


  std::cout << "   [Scheduler]...........returniing selscted device." << std::endl;

  return devices.back();;
}
