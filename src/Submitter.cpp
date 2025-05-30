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
// Start threads to consume from each queue concurrently
std::vector<std::thread> threadsConnections;
void joinThreadsConnections() {
  for (auto &thread : threadsConnections)
    if (thread.joinable())
      thread.join();
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
    logger->warn("Stopping the Submitter...");
    // Close the connections
    logger->warn("Closing connections to RabbitMQ");
    joinThreadsConnections();
    mqss::Logger::cleanup();
    exit(0);
  }
}

void returnResult(const std::string &resultsMessage) {
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT, QUEUE_HPC_OFFLOADER,
                              AMQP_USER, AMQP_PASSWORD);
  forwardQueue.publishMessage(resultsMessage, true);
}

void submit(QuantumTask quantumTask) {
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT, QUEUE_SUBMITTER_BACKEND,
                              AMQP_USER, AMQP_PASSWORD);
  std::cout << "Received task with id: " << quantumTask.task_id << std::endl;
  for (auto task : quantumTask.circuit_files)
    std::cout << task << std::endl;
  // submit now to mock device
  json taskJson = dumpQuantumTaskToJson(quantumTask);
  forwardQueue.publishMessage(taskJson.dump(), true);
}

int main(int argc, char *argv[]) {
  // Install the signal handler for SIGINT (Ctrl+C)
  std::signal(SIGINT, signalHandler);
  mqss::Logger::init(FILE_LOGGER_SUBMITTER, LOGGER_SUBMITTER);
  // Get the logger instance
  auto logger = mqss::Logger::getLogger();
  RabbitMQServer queueListener(AMQP_SERVER, AMQP_PORT,
                               QUEUE_TRANSPILER_SUBMITTER, AMQP_USER,
                               AMQP_PASSWORD);
  logger->info("Running up the Submitter...");
  // tell the offloaderListener to start to consume
  queueListener.startToConsume();
  while (true) {
    logger->info("Waiting for a new job...");
    std::string message;
    queueListener.consumeMessage(message);
    if (message.find("__global__") != std::string::npos) {
      // message comes from a device and send back
      json jsonResults = json::parse(message);
      std::string taskId = jsonResults["task_id"];
      threadsConnections.push_back(std::thread(returnResult, message));
      std::cout << "Received results for task with id: " << taskId << std::endl;
    } else {
      QuantumTask quantumTask = dumpJsonToQuantumTask(message.c_str());
      std::string taskId = quantumTask.task_id;
      threadsConnections.push_back(std::thread(submit, std::move(quantumTask)));
      logger->info("Processing new task with id: {}", taskId);
    }
  }
  // Ensure the logger is properly destroyed
  joinThreadsConnections();
  mqss::Logger::cleanup();
  return 1;
}
