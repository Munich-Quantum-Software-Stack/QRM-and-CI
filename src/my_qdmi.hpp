#ifndef MY_QDMI_HPP
#define MY_QDMI_HPP

#include "../include/QuantumResourceManager.hpp"
struct QuantumTask;

struct Queue {
  std::string platform;             // Name of the platform
  std::vector<QuantumTask *> tasks; // List of tasks in the queue
  float end_time;                   // end_time of the full queue

  Queue(std::string platform) : platform(platform), end_time(0.) {}

  void insertTask(int position, QuantumTask *task, float duration) {
    if (position >= 0 && position <= tasks.size()) {
      tasks.insert(tasks.begin() + position, task);
      end_time = end_time + duration;
    } else {
      // Handle error: position out of range
    }
  }
};

#endif // MY_QDMI_HPP
