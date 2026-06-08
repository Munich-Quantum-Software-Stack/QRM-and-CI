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
    Implementation of the Submitter.
    At the moment this block connects to a single mock backend.
    The submitter lowers code to QIR and submit it.
    TODO: Include QDMI to communicate to remote backends

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#include "cudaq/Optimizer/Transforms/Passes.h"
#include "mqss/ConnectionHandler.hpp"
#include "mqss/LoggerHandler.hpp"
#include "mqss/common/Logger.hpp"
#include "mqss/common/QuantumTask.hpp"
#include "mqss/common/RabbitMQServer.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <map>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Target/LLVMIR/Export.h>
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
#include "cudaq/Optimizer/CodeGen/OptUtils.h"
#include "cudaq/Optimizer/CodeGen/Passes.h"

// MQSS includes
#include "driver/qdmi_example_driver.h"
#include "qdmi/client.h"
#include "qdmi/constants.h"
#include "qdmi/device.h"

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

// Trim leading and trailing whitespace
std::string trim(const std::string &str) {
  size_t first =
      str.find_first_not_of(" \t\n\r\f\v"); // Find first non-whitespace
  if (first == std::string::npos) {
    return ""; // If no non-whitespace characters, return an empty string
  }
  size_t last = str.find_last_not_of(" \t\n\r\f\v"); // Find last non-whitespace
  return str.substr(first, (last - first + 1));      // Return trimmed substring
}

std::string getKernelName(const std::string &program) {
  std::regex patternKernel("func\\.func @__nvqpp__mlirgen__([^\\(\\)]+)\\(\\)");
  std::smatch matches;
  if (!std::regex_search(program, matches, patternKernel))
    throw std::runtime_error(
        "Error, no kernel function name found on the given Quake program...");
  std::string kernelName = matches[1];
  // Find the position of the substring
  size_t pos = kernelName.find(CUDAQ_GEN_PREFIX_NAME);
  // If the substring is found, erase it
  if (pos != std::string::npos) {
    kernelName.erase(pos, std::string(CUDAQ_GEN_PREFIX_NAME).length());
  }
  return trim(kernelName);
}

std::string lowerQuakeCode(const std::string &circuit,
                           const std::string &kernelName) {
  auto [m_module, contextPtr] = extractMLIRContext(circuit);

  mlir::MLIRContext &context = *contextPtr;
  // Extract the kernel name
  auto func = m_module.lookupSymbol<mlir::func::FuncOp>(
      std::string(CUDAQ_GEN_PREFIX_NAME + kernelName));

  mlir::PassManager pm(m_module->getContext());
  cudaq::opt::addAggressiveInlining(pm);
  cudaq::opt::createTargetFinalizePipeline(pm);
  cudaq::opt::addJITPipelineConvertToQIR(pm, "qir-base");

  // ✅ actually run the passes
  if (failed(pm.run(m_module)))
    throw std::runtime_error("Failed to lower Quake to QIR");

  llvm::LLVMContext llvmContext;
  llvmContext.setOpaquePointers(false);
  auto llvmModule = mlir::translateModuleToLLVMIR(m_module, llvmContext);

  // Optimize
  // auto optPipeline = mlir::makeOptimizingTransformer(3, 0, nullptr);
  // if (auto err = optPipeline(m_module.get()))
  //   throw std::runtime_error("Failed to optimize LLVM IR");

  // Print directly to string
  std::string loweredCode;
  {
    llvm::raw_string_ostream os(loweredCode);
    os << *llvmModule;
  }
  return loweredCode;
}

void returnResult(const std::string &resultsMessage) {
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT, QUEUE_HPC_OFFLOADER,
                              AMQP_USER, AMQP_PASSWORD);
  forwardQueue.publishMessage(resultsMessage, true);
}

static std::vector<QDMI_Site> getDeviceCouplingMap(QDMI_Device device) {
  // Step 1: get the size
  size_t size_ret = 0;
  int ret = -223;
 ret =  QDMI_device_query_device_property(device,
                                           QDMI_DEVICE_PROPERTY_COUPLINGMAP, 0,
                                           nullptr, &size_ret);

  std::cout << "-->Query coupling map size: " << size_ret << "\n";
  assert(ret == QDMI_SUCCESS);
  // size_ret = 20 * sizeof(QDMI_Site) for the cxx device
  size_t num_entries = size_ret / sizeof(QDMI_Site); // = 20
  size_t num_pairs = num_entries / 2;                // = 10

  // Step 2: retrieve
  std::vector<QDMI_Site> coupling_map(num_entries);
  ret = QDMI_device_query_device_property(
             device, QDMI_DEVICE_PROPERTY_COUPLINGMAP, size_ret,
             static_cast<void *>(coupling_map.data()),
             nullptr);

  std::cout << "-->Query coupling map entries: " << coupling_map.size() << "\n";
  assert(ret == QDMI_SUCCESS);
  // Step 3: iterate over pairs
  for (size_t i = 0; i < num_entries; i += 2) {
    QDMI_Site src = coupling_map[i];
    QDMI_Site dst = coupling_map[i + 1];

    // query the index of each site
    uint64_t src_id = 0, dst_id = 0;
    QDMI_device_query_site_property(device, src, QDMI_SITE_PROPERTY_INDEX,
                                    sizeof(uint64_t), &src_id, nullptr);
    QDMI_device_query_site_property(device, dst, QDMI_SITE_PROPERTY_INDEX,
                                    sizeof(uint64_t), &dst_id, nullptr);

    std::cout << src_id << " -> " << dst_id << "\n";
  }
  return coupling_map;
}

static QDMI_Job createAndsubmitJob(const std::string TEST_CIRCUIT) {

  QDMI_Job job = nullptr;
  int num_shots = 1000;

  std::cout << "Initializing QDMI driver...\n";

  setenv("QDMI_CONF", "qdmi.conf", 1);

  int ret = QDMI_driver_init();
  assert(ret == QDMI_SUCCESS);
  // Immediately check what the conf file actually contains
  std::ifstream conf("qdmi.conf");
  if (!conf.is_open()) {
    std::cout << "ERROR: qdmi.conf not found!\n";
  } else {
    std::cout << "=== qdmi.conf ===\n"
              << conf.rdbuf() << "\n=================\n";
  }

  QDMI_Session session = nullptr;
  ret = QDMI_session_alloc(&session);
  assert(ret == QDMI_SUCCESS);

  // Empty token = read-only; non-empty token = read/write
  const char *token = "XX12Mayi98"; // read-only
  ret = QDMI_session_set_parameter(session, QDMI_SESSION_PARAMETER_TOKEN,
                                    strlen(token) + 1, token);
  assert(ret == QDMI_SUCCESS);

  // Initialize QDMI session
  ret = QDMI_session_init(session); // device sessions are created here
  std::cout << "QDMI_session_init() : " << ret << " session = " << session << "\n";
  assert(ret == QDMI_SUCCESS);

  // Query the number of devices
  size_t size_ret = 0;
  ret = QDMI_session_query_session_property(
             session, QDMI_SESSION_PROPERTY_DEVICES, 0, nullptr, &size_ret);

  assert(ret == QDMI_SUCCESS);
  
  size_t num_devices = size_ret / sizeof(QDMI_Device);
  std::vector<QDMI_Device> devices(num_devices);
  ret = QDMI_session_query_session_property(
             session, QDMI_SESSION_PROPERTY_DEVICES, size_ret,
             static_cast<void *>(devices.data()), nullptr);

  std::cout << "--> QDMI Num Devices: " << devices.size() << "\n";
  assert(ret == QDMI_SUCCESS);

  
  // Create a Job
  QDMI_Device dev = devices[0];
  getDeviceCouplingMap(dev);

  QDMI_device_create_job(dev, &job);
  

  const auto format = QDMI_PROGRAM_FORMAT_QIRBASESTRING;
  QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_PROGRAMFORMAT,
                         sizeof(QDMI_Program_Format), &format);
  QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_PROGRAM,
                         TEST_CIRCUIT.size() + 1, TEST_CIRCUIT.c_str());
  if (num_shots > 0) {
    QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_SHOTSNUM, sizeof(size_t),
                           &num_shots);
  }
  QDMI_job_submit(job);
  QDMI_job_wait(job, 0);
  // Teardown (in reverse order)
  QDMI_session_free(session);
  QDMI_driver_shutdown(); // dlclose() happens here

  return job;
}

void submit(QuantumTask quantumTask) {
  RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT, QUEUE_SUBMITTER_BACKEND,
                              AMQP_USER, AMQP_PASSWORD);
#ifdef DEBUG
  std::cout << "Received task with id: " << quantumTask.task_id << std::endl;
  for (auto task : quantumTask.circuit_files)
    std::cout << task << std::endl;
#endif
  // the conversion to QIR happens inside the submittter
  // Extract job details from the request body
  for (int i = 0; i < quantumTask.circuit_files.size(); i++) {
    int jobCount = quantumTask.n_shots;

    // Program in MLIR dialect format. Here Quake
    std::string program = quantumTask.circuit_files[i];
    std::string kernelName = getKernelName(program);

    std::string qirCode = lowerQuakeCode(program, kernelName);
    createAndsubmitJob(qirCode);
    // update QIR code into the quantumTask
    quantumTask.circuit_files[i] = qirCode;
  }
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
#ifdef DEBUG
      std::cout << "Received results for task with id: " << taskId << std::endl;
#endif
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
