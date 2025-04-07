/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */
// #include "mqss/QuantumResourceManager.hpp"
#include "mqss/ConnectionHandler.hpp"
#include "mqss/LoggerHandler.hpp"
#include "mqss/common/Logger.hpp"
#include "mqss/common/QuantumTask.hpp"
#include "mqss/common/RabbitMQServer.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <thread>

using json = nlohmann::json;
using namespace mqss;

// Enum definition for task states
enum class TaskStatus { RUNNING, CANCELLED, COMPLETED, UNKNOWN };

// Function to convert enum to string (for easy printing)
const char *to_string(TaskStatus status) {
  switch (status) {
  case TaskStatus::RUNNING:
    return "RUNNING";
  case TaskStatus::CANCELLED:
    return "CANCELLED";
  case TaskStatus::COMPLETED:
    return "COMPLETED";
  case TaskStatus::UNKNOWN:
    return "UNKNOWN";
  default:
    return "UNKNOWN";
  }
}

std::map<boost::uuids::uuid,
         std::pair<std::string, std::unordered_map<int, int>>>
    finishedJobs;
std::map<boost::uuids::uuid, TaskStatus> statusQuantumJobs;
std::shared_mutex jobsMutex;
// Start threads to consume from each queue concurrently
std::vector<std::thread> threadsConnections;

void joinThreadsConnections() {
  for (auto &thread : threadsConnections)
    if (thread.joinable())
      thread.join();
}

void addJobStatus(const boost::uuids::uuid &uuid, TaskStatus status) {
  std::unique_lock<std::shared_mutex> lock(
      jobsMutex); // Exclusive lock for writing
  statusQuantumJobs[uuid] = status;
}

// Thread-safe function to check if a task exists
bool jobStatusExists(const boost::uuids::uuid &uuid) {
  std::lock_guard<std::shared_mutex> lock(jobsMutex); // Locking the mutex
  return statusQuantumJobs.find(uuid) !=
         statusQuantumJobs.end(); // Safe check for existence
}

TaskStatus getJobStatus(const boost::uuids::uuid &taskId) {
  std::shared_lock<std::shared_mutex> lock(
      jobsMutex); // Shared lock for reading
  auto it = statusQuantumJobs.find(taskId);
  if (it != statusQuantumJobs.end()) {
    return it->second; // Return the task
  }
  return TaskStatus::UNKNOWN;
}

/**
 * @brief Function for the graceful termination of this daemon closing
 * its own socket before exiting
 * @param signum Number of the interrupt signal
 */
void signalHandler(int signum) {
  auto logger = mqss::Logger::getLogger();
  if (signum == SIGINT) {
    int err;
    logger->warn("Stopping the QRM daemon");
    // Close the connections
    logger->warn("Closing connections to RabbitMQ");
    //  Finalize the QDMI session
    logger->warn("Finalizing QDMI session");
    // err = QDMI_session_finalize(session);
    // CHECK_ERR(err, "QDMI_session_finalize");
    joinThreadsConnections();
    mqss::Logger::cleanup();
    exit(0);
  }
}

void processTask(QuantumTask quantumTask, const std::string &replyQueue,
                 const std::string &correlationId, boost::uuids::uuid taskId) {
  RabbitMQServer replyServer(AMQP_SERVER, AMQP_PORT, QUEUE_OFFLOADER_LISTENER,
                             AMQP_USER, AMQP_PASSWORD);
  quantumTask.task_id = taskId;
  // dumpQuantumTask(quantumTask);
  json taskJson = dumpQuantumTaskToJson(std::ref(quantumTask));
  // register job
  addJobStatus(taskId, TaskStatus::RUNNING);
  replyServer.publishMessage(replyQueue, taskJson.dump(), correlationId, true);
  // now I pass the task to the Quantum Agnostic Pass Runner
  RabbitMQServer toAgnosticPasses(AMQP_SERVER, AMQP_PORT,
                                  QUEUE_QRM_AGNOSTIC_PASS_RUNNER, AMQP_USER,
                                  AMQP_PASSWORD);
}

void processCheckStatusTask(QuantumTask quantumTask,
                            const std::string &replyQueue,
                            const std::string &correlationId) {
  RabbitMQServer replyServer(AMQP_SERVER, AMQP_PORT, QUEUE_OFFLOADER_LISTENER,
                             AMQP_USER, AMQP_PASSWORD);
  TaskStatus statusJob = getJobStatus(quantumTask.task_id);
  nlohmann::json statusJson = {{"status", to_string(statusJob)}};
  replyServer.publishMessage(replyQueue, statusJson.dump(), correlationId,
                             true);
  // at this point I have to pass the task to the queue connecting to the
  // agnostic pass runner
}

int main(int argc, char *argv[]) {
  // Install the signal handler for SIGINT (Ctrl+C)
  std::signal(SIGINT, signalHandler);
  mqss::Logger::init(FILE_LOGGER_QRM, LOGGER_QRM);
  // Get the logger instance
  auto logger = mqss::Logger::getLogger();
  RabbitMQServer offloaderListener(AMQP_SERVER, AMQP_PORT,
                                   QUEUE_OFFLOADER_LISTENER, AMQP_USER,
                                   AMQP_PASSWORD);
  logger->info("Running up the Quantum Resource Manager (QRM)");
  // tell the offloaderListener to start to consume
  offloaderListener.startToConsume();
  while (true) {
    logger->info("Waiting for a new job...");
    amqp_envelope_t envelope;
    std::string message, replyQueue, correlationId;
    offloaderListener.consumeMessage(envelope, message, replyQueue,
                                     correlationId);
    boost::uuids::random_generator generator;
    QuantumTask quantumTask = dumpJsonToQuantumTask(message.c_str());
    if (quantumTask.task_id == boost::uuids::nil_uuid()) {
      // Generate a new UUID
      boost::uuids::uuid newTaskId = generator();
      threadsConnections.push_back(
          std::thread(processTask, std::move(quantumTask), replyQueue,
                      correlationId, newTaskId));
      logger->info("Processing new task with id: {}",
                   boost::uuids::to_string(newTaskId));
    } else {
      boost::uuids::uuid taskId = quantumTask.task_id;
      threadsConnections.push_back(std::thread(processCheckStatusTask,
                                               std::move(quantumTask),
                                               replyQueue, correlationId));
      logger->info("Checking status of task with id: {}",
                   boost::uuids::to_string(taskId));
    }
  }
  // Ensure the logger is properly destroyed
  joinThreadsConnections();
  mqss::Logger::cleanup();
  return 1;
}
