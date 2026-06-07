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
        Example of a mock backend that receives QIR that is executed using
        cudaq in python as simulator.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

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
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <regex>
#include <shared_mutex>
#include <thread>
// llvm includes
#include "llvm/Bitcode/BitcodeReader.h"

#include <llvm/Support/Base64.h>
// mlir includes
#include "mlir/ExecutionEngine/OptUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Parser/Parser.h"
// cudaq includes
#include "common/JIT.h"
#include "common/RuntimeMLIR.h"
#include "createJob.h"
// #include "cudaq/Optimizer/CodeGen/Pipelines.h"

#define CUDAQ_GEN_PREFIX_NAME "__nvqpp__mlirgen__"
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

void mockBackend(QuantumTask quantumTask) {
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT,
                              QUEUE_TRANSPILER_SUBMITTER, AMQP_USER,
                              AMQP_PASSWORD);
#ifdef DEBUG
  std::cout << "Received task with id: " << quantumTask.task_id << std::endl;
  for (auto task : quantumTask.circuit_files)
    std::cout << task << std::endl;
#endif
  // Extract job details from the request body
  int jobCount = quantumTask.n_shots;
  // Read QIR from job
  std::string qirCode = quantumTask.circuit_files[0];
  // Simulate kernel function and qubit processing
  std::ofstream outFile(std::string("./tempCircuit.txt"),
                        std::ios::out | std::ios::trunc);
  if (!outFile.is_open())
    throw std::runtime_error("Failed to open file!");
  outFile << qirCode;
  outFile.close();
  // Construct the system call to run Python with the input
  std::string command =
      std::string("python3 ./SimulateKernelLoweredToLLVM.py -c tempCircuit.txt "
                  "-o tempResults.txt -s 100");
  // Execute the command
  int result = system(command.c_str());
  // Check the result (0 means success)
  if (result != 0)
    throw std::runtime_error("Python script execution failed!");
  // now read the results from file
  std::ifstream file(std::string("./tempResults.txt"));
  // Check if the file was opened successfully
  if (!file)
    throw std::runtime_error("File could not be opened!");
  // Create a stringstream object to read the file content
  std::stringstream bufferResult;
  // Read the file content into the stringstream
  bufferResult << file.rdbuf();
  // Convert the stringstream into a string
  std::string resultCircuit = bufferResult.str();
  file.close();
  std::remove("./tempCircuit.txt");
  std::remove("./tempResults.txt");
#ifdef DEBUG
  std::cout << "Results:" << std::endl << resultCircuit << std::endl;
#endif
  json results = {{"task_id", quantumTask.task_id}, {"results", resultCircuit}};
  // after processing, move forward the quantum task
  forwardQueue.publishMessage(results.dump(), true);
}

static HardwareSession* setUpNewSession() {
  char *hostname = getenv(LRZ_HOST_URL);
  char *token = getenv(MQP_SECRET_TOKEN);
  int err;
  EXIT_ON_FAIL(LRZ_QDMI_device_initialize(), "Failed to initialize the device");
  size_t hardware_size;
  EXIT_ON_FAIL(
      LRZ_QDMI_device_session_query_device_property(
          nullptr, QDMI_DEVICE_PROPERTY_CUSTOM1, 0, nullptr, &hardware_size),
      "Could not fetch the hardwares");
  std::string hardwares(hardware_size - 1, '\0');

  EXIT_ON_FAIL(LRZ_QDMI_device_session_query_device_property(
                   nullptr, QDMI_DEVICE_PROPERTY_CUSTOM1, 0,
                   static_cast<void *>(hardwares.data()), nullptr),
               "Could not fetch the hardwares");

  std::regex del(";");
  std::sregex_token_iterator hardware_iterator(hardwares.begin(),
                                               hardwares.end(), del, -1);
  std::sregex_token_iterator hardware_iterator_end;

  HardwareSession *hardware_session = NAME_SESSIONS[hardware_iterator++->str()];
  LRZ_QDMI_Device_Session device_session;
  LRZ_QDMI_device_session_alloc(&device_session);

  LRZ_QDMI_device_session_set_parameter(
      device_session, QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
      strlen(hostname) * sizeof(char) + 1, hostname);

  LRZ_QDMI_device_session_set_parameter(
      device_session, QDMI_DEVICE_SESSION_PARAMETER_TOKEN,
      strlen(token) * sizeof(char) + 1, token);

  LRZ_QDMI_device_session_set_parameter(
      device_session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1,
      hardware_session->name.size() * sizeof(char) + 1,
      hardware_session->name.data());

  err = LRZ_QDMI_device_session_init(device_session);
  if (err == QDMI_SUCCESS) {
    hardware_session->session =
        std::make_shared<LRZ_QDMI_Device_Session>(device_session);
  }
  return hardware_session;
}

void createLRZJob(std::string test_circuit) {

  HardwareSession *hardware_session = setUpNewSession();
  if (hardware_session->session == nullptr) {
    std::cout << "Empty hardware session!!\n";
    return;
  }
  std::shared_ptr<LRZ_QDMI_Device_Session> session_ptr =
      hardware_session->session;
  LRZ_QDMI_Device_Session session = *(session_ptr.get());

  LRZ_QDMI_Device_Job job = nullptr;
  size_t nShot = 50;
  const QDMI_Program_Format qasmFormat = QDMI_PROGRAM_FORMAT_QASM2;

  const char *c_t_c = test_circuit.c_str();
  QDMI_Job_Status *job_status =
      (QDMI_Job_Status *)malloc(sizeof(QDMI_Job_Status));
  int create_device_Job = LRZ_QDMI_device_session_create_device_job(session, &job);

  assert((create_device_Job == QDMI_STATUS::QDMI_SUCCESS) && "Device job could not be created");
              
}

int main(int argc, char *argv[]) {
  // Install the signal handler for SIGINT (Ctrl+C)
  std::signal(SIGINT, signalHandler);
  mqss::Logger::init(FILE_LOGGER_MOCK_DEVICE, LOGGER_MOCK_DEVICE);
  // Get the logger instance
  auto logger = mqss::Logger::getLogger();
  RabbitMQServer queueListener(AMQP_SERVER, AMQP_PORT, QUEUE_SUBMITTER_BACKEND,
                               AMQP_USER, AMQP_PASSWORD);
  logger->info("Running up Mock Backend!...");
  // tell the offloaderListener to start to consume
  queueListener.startToConsume();
  while (true) {
    logger->info("Waiting for a new job...");
    std::string message;
    queueListener.consumeMessage(message);
    QuantumTask quantumTask = dumpJsonToQuantumTask(message.c_str());
    std::string taskId = quantumTask.task_id;
    threadsConnections.push_back(
        std::thread(mockBackend, std::move(quantumTask)));
    logger->info("Processing new task with id: {}", taskId);
  }
  // Ensure the logger is properly destroyed
  joinThreadsConnections();
  mqss::Logger::cleanup();
  return 1;
}
