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
  float duration;            // the predicted duration of the job

  // Default constructor
  QuantumTask() : parent(nullptr), TSM(nullptr) {}
};

struct Queue {
  std::string platform;             // Name of the platform
  std::vector<QuantumTask *> tasks; // List of tasks in the queue
  float end_time;                   // end_time of the full queue

  Queue(std::string platform) : platform(platform), end_time(0.) {}

  void insertTask(int position, QuantumTask *task) {
    if (position >= 0 && position <= tasks.size()) {
      tasks.insert(tasks.begin() + position, task);
      end_time = end_time + task->duration;
    } else {
      // Handle error: position out of range
    }
  }
};

#endif // MY_QDMI_HPP
