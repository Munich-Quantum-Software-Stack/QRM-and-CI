#ifndef MY_QDMI_HPP
#define MY_QDMI_HPP

using llvm::orc::ThreadSafeModule;

struct QuantumTask {
  int task_id;
  int n_qbits;
  int n_shots;
  std::string circuit_file;
  std::string circuit_file_type;
  std::string result_destination;
  std::vector<std::string> preferred_qpus;
  std::string scheduled_qpu;
  int priority;
  int optimisation_level;
  bool no_modify;
  bool transpiler_flag;
  int result_type;
  std::string submit_time;
  std::string circuit_qiskit;
  std::string additional_information;
  std::string change_generator;
  std::string change_selector;
  std::string change_scheduler;
  ThreadSafeModule *TSM;     // QIR quantum circuit
  const QuantumTask *parent; // set by generator if the task is a sub-task
  float end_time;            // the end time of the job
  float duration;            // the predicted duration of the job

  // Default constructor
  QuantumTask() : parent(nullptr), TSM(nullptr) {}

  float updateEndTime(float new_end_time) {
    if (this->parent != nullptr) {
      this->parent->updateEndTime(new_end_time);
    }
    if (new_end_time > this->end_time) {
      this->end_time = new_end_time;
    }
    return this->end_time;
  }
};

struct Queue {
  std::string platform;             // Name of the platform
  std::vector<QuantumTask *> tasks; // List of tasks in the queue
  float *end_time;                  // Pointer to the end_time of the last task

  Queue(std::string platform) : platform(platform), end_time(nullptr) {}

  void insertTask(int position, QuantumTask *task) {
    if (position >= 0 && position <= tasks.size()) {
      tasks.insert(tasks.begin() + position, task);
      if (position == tasks.size() - 1) {
        end_time = &(task->end_time); // Update end_time pointer
      }
    } else {
      // Handle error: position out of range
    }
  }
};

#endif // MY_QDMI_HPP
