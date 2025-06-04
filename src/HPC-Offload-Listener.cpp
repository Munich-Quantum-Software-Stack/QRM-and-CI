/* This code and any associated documentation is provided "as is"

Copyright 2024 Munich Quantum Software Stack Project

Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
"License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

TODO

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-------------------------------------------------------------------------
  author Martin Letras
  date   April 2025
  version 1.0
  brief
    Implementation of the hpc-offload listener. This is the entry point of the
    MQSS when jobs are submitted using hpc resources. The access to the MQSS
    is via rabbitmqs.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#include "mqss/ConnectionHandler.hpp"
#include "mqss/LoggerHandler.hpp"
#include "mqss/common/Logger.hpp"
#include "mqss/common/QuantumTask.hpp"
#include "mqss/common/RabbitMQServer.hpp"
#include "mqss/common/TaskStatus.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <regex>
#include <shared_mutex>
#include <thread>

using json = nlohmann::json;
using namespace mqss;

std::map<boost::uuids::uuid, std::unordered_map<int, int>> finishedJobs;
std::shared_mutex finishedJobsMutex;
std::map<boost::uuids::uuid, TaskStatus> statusQuantumJobs;
std::shared_mutex jobsMutex;
// Start threads to consume from each queue concurrently
std::vector<std::thread> threadsConnections;

void joinThreadsConnections() {
  for (auto &thread : threadsConnections)
    if (thread.joinable())
      thread.join();
}

void addJobStatus(const std::string &uuidStr, TaskStatus status) {
  boost::uuids::string_generator gen;
  std::unique_lock<std::shared_mutex> lock(
      jobsMutex); // Exclusive lock for writing
  statusQuantumJobs[gen(uuidStr)] = status;
}

// Thread-safe function to check if a task exists
bool jobStatusExists(const boost::uuids::uuid &uuid) {
  std::lock_guard<std::shared_mutex> lock(jobsMutex); // Locking the mutex
  return statusQuantumJobs.find(uuid) !=
         statusQuantumJobs.end(); // Safe check for existence
}

TaskStatus getJobStatus(const std::string &taskIdStr) {
  boost::uuids::uuid taskId;
  try {
    boost::uuids::string_generator gen;
    taskId = gen(taskIdStr);
  } catch (std::exception &e) {
    return TaskStatus::UNKNOWN;
  }
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

std::unordered_map<std::string, std::unordered_map<int, int>>
parseStringToMap(const std::string &input) {
  std::unordered_map<std::string, std::unordered_map<int, int>> resultMap;
  // Regex to match outer keys and their corresponding { ... } content
  std::regex outerRegex(R"((\w+)\s*:\s*\{([^}]+)\})");
  std::smatch outerMatch;
  std::string::const_iterator searchStart(input.cbegin());
  while (std::regex_search(searchStart, input.cend(), outerMatch, outerRegex)) {
    std::string outerKey =
        outerMatch[1]; // Capture outer key, e.g., "__global__"
    std::string innerContent =
        outerMatch[2]; // Capture inner content, e.g., "0:479 1:521"
    // Parse the inner map
    std::unordered_map<int, int> innerMap;
    std::regex innerRegex(R"((\d+)\s*:\s*(\d+))");
    std::smatch innerMatch;
    std::string::const_iterator innerStart(innerContent.cbegin());
    while (std::regex_search(innerStart, innerContent.cend(), innerMatch,
                             innerRegex)) {
      int key = std::stoi(innerMatch[1]);
      int value = std::stoi(innerMatch[2]);
      innerMap[key] = value;
      innerStart = innerMatch.suffix().first; // Move to the next match
    }
    // Add the parsed inner map to the result
    resultMap[outerKey] = innerMap;
    searchStart = outerMatch.suffix().first; // Move to the next outer match
  }
  return resultMap;
}

void processTask(QuantumTask quantumTask, const std::string &replyQueue,
                 const std::string &correlationId, std::string taskId) {
  RabbitMQServer replyServer(AMQP_SERVER, AMQP_PORT, QUEUE_HPC_OFFLOADER,
                             AMQP_USER, AMQP_PASSWORD);
  quantumTask.task_id = taskId;
  // dumpQuantumTask(quantumTask);
  json taskJson = dumpQuantumTaskToJson(std::ref(quantumTask));
  // register job
  addJobStatus(taskId, TaskStatus::RUNNING);
  replyServer.publishMessage(replyQueue, taskJson.dump(), correlationId, true);
  // now I pass the task to the Quantum Agnostic Pass Runner
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT,
                              QUEUE_QRM_AGNOSTIC_PASS_RUNNER, AMQP_USER,
                              AMQP_PASSWORD);
  forwardQueue.publishMessage(taskJson.dump(), true);
}

void processCheckStatusTask(const std::string &taskId,
                            const std::string &replyQueue,
                            const std::string &correlationId) {
  RabbitMQServer replyServer(AMQP_SERVER, AMQP_PORT, QUEUE_MQP_OFFLOADER,
                             AMQP_USER, AMQP_PASSWORD);
  TaskStatus statusJob = getJobStatus(taskId);
  nlohmann::json jsonResponse;

  if (statusJob == mqss::TaskStatus::RUNNING)
    jsonResponse = {{"status", "running"}};
  if (statusJob == mqss::TaskStatus::CANCELLED ||
      statusJob == mqss::TaskStatus::UNKNOWN)
    jsonResponse = getErrorAnswer(404, "Job not found");
  if (statusJob == mqss::TaskStatus::COMPLETED) {
    // Retrieve the job data (name and counts)
    boost::uuids::string_generator gen;
    auto counts = finishedJobs[gen(taskId)];
    // Prepare the result data by expanding the counts
    std::vector<int> retData;
    for (const auto &[bits, count] : counts) {
      for (int i = 0; i < count; ++i) {
        retData.push_back(bits);
      }
    }
    // Convert the result data to a string list
    std::vector<std::string> stringResults;
    for (int bits : retData)
      stringResults.push_back(std::to_string(bits));
    // Create the final response JSON object
    nlohmann::json resultResponse;
    jsonResponse["status"] = "completed";
    jsonResponse["results"]["MOCK_SERVER_RESULTS"] = stringResults;
  }
  replyServer.publishMessage(replyQueue, jsonResponse.dump(), correlationId,
                             true);
}

void addFinishedJob(
    boost::uuids::uuid taskId,
    std::unordered_map<std::string, std::unordered_map<int, int>> results) {
  std::unique_lock<std::shared_mutex> lock(
      finishedJobsMutex); // Exclusive lock for writing
  finishedJobs[taskId] = results[std::string("__global__")];
}

void saveResults(const std::string &resultsMessage) {
  json jsonResults = json::parse(resultsMessage);
  std::string taskIdStr = jsonResults["task_id"];
  boost::uuids::string_generator gen;
  boost::uuids::uuid taskId;
  try {
    taskId = gen(taskIdStr);
  } catch (const std::exception &e) {
    std::cerr << "Invalid UUID string: " << e.what() << std::endl;
    exit(1);
  }
  std::cout << "Received message " << std::endl
            << jsonResults["results"] << std::endl;
  // Simulate results (in the original, this comes from some quantum function)
  std::unordered_map<std::string, std::unordered_map<int, int>> results =
      parseStringToMap(jsonResults["results"]);
  // Print map contents
  for (const auto &outer_pair : results) {
    std::cout << "Key: " << outer_pair.first << std::endl;
    for (const auto &inner_pair : outer_pair.second) {
      std::cout << "  Inner Key: " << inner_pair.first
                << ", Value: " << inner_pair.second << std::endl;
    }
  }

  // Store the created job in the global jobs dictionary
  addFinishedJob(taskId, results);
  // finishedJobs[taskId] = results[std::string("__global__")];
  addJobStatus(taskIdStr, TaskStatus::COMPLETED);
}

int main(int argc, char *argv[]) {
  // Install the signal handler for SIGINT (Ctrl+C)
  std::signal(SIGINT, signalHandler);
  mqss::Logger::init(FILE_LOGGER_QRM, LOGGER_QRM);
  // Get the logger instance
  auto logger = mqss::Logger::getLogger();
  RabbitMQServer offloaderListener(AMQP_SERVER, AMQP_PORT, QUEUE_HPC_OFFLOADER,
                                   AMQP_USER, AMQP_PASSWORD);
  logger->info("Running up the Quantum Resource Manager (QRM)");
  // tell the offloaderListener to start to consume
  offloaderListener.startToConsume();

  while (true) {
    logger->info("Waiting for a new job...");
    amqp_envelope_t envelope;
    std::string message, replyQueue, correlationId;
    offloaderListener.consumeMessage(envelope, message, replyQueue,
                                     correlationId);
    try {
      // try to parse the received message, if is a json then I have to process
      // it as a new job
      auto jsonTask = nlohmann::json::parse(message);
      if (message.find("__global__") != std::string::npos) {
        std::string taskId = jsonTask["task_id"];
        threadsConnections.push_back(std::thread(saveResults, message));
        std::cout << "Received results for task with id: " << taskId
                  << std::endl;
      } else {
        boost::uuids::random_generator generator;
        QuantumTask quantumTask = dumpJsonToQuantumTask(message.c_str());
        boost::uuids::uuid newTaskId = generator();
        threadsConnections.push_back(
            std::thread(processTask, std::move(quantumTask), replyQueue,
                        correlationId, boost::uuids::to_string(newTaskId)));
        logger->info("Processing new task with id: {}",
                     boost::uuids::to_string(newTaskId));
      }
    } catch (const nlohmann::json::parse_error &e) {
      // the message is just a regular uuid for the case when asking the status
      // of a job
      threadsConnections.push_back(std::thread(processCheckStatusTask, message,
                                               replyQueue, correlationId));
      logger->info("Checking status of task with id: {}", message);
    }
  }
  // Ensure the logger is properly destroyed
  joinThreadsConnections();
  mqss::Logger::cleanup();
  return 1;
}
