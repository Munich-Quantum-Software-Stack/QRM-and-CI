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
#include "cudaq/Optimizer/CodeGen/Pipelines.h"

#define CUDAQ_GEN_PREFIX_NAME "__nvqpp__mlirgen____"
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

  auto translation = cudaq::getTranslation("qir-base");

  std::string codeStr;
  {
    llvm::raw_string_ostream outStr(codeStr);
    m_module.getContext()->disableMultithreading();
    if (failed(translation(m_module, outStr, "", false, false, false)))
      throw std::runtime_error("Could not successfully translate to qir-base");
  }

  std::vector<char> decodedBase64Output;
  // Decode the Base64 string
  if (llvm::decodeBase64(codeStr, decodedBase64Output))
    throw std::runtime_error("Error decoding Base64 string");

  std::string decodedBase64Kernel =
      std::string(decodedBase64Output.data(), decodedBase64Output.size());
  // decode the LLVM byte code to string
  llvm::LLVMContext contextLLVM;
  contextLLVM.setOpaquePointers(false);
  auto memoryBuffer = llvm::MemoryBuffer::getMemBuffer(decodedBase64Kernel);

  llvm::Expected<std::unique_ptr<llvm::Module>> moduleOrErr =
      llvm::parseBitcodeFile(*memoryBuffer, contextLLVM);
  std::error_code ec = llvm::errorToErrorCode(moduleOrErr.takeError());
  if (ec)
    throw std::runtime_error(
        "Compiler::Error parsing bitcode..."); // when debbugin dump:
                                               // ec.message())
  // Successfully parsed
  std::unique_ptr<llvm::Module> moduleConverted = std::move(*moduleOrErr);

  auto optPipeline = mlir::makeOptimizingTransformer(
      /*optLevel=*/3, /*sizeLevel=*/0,
      /*targetMachine=*/nullptr);
  if (auto err = optPipeline(moduleConverted.get()))
    throw std::runtime_error("getQIR Failed to optimize LLVM IR ");

  std::string loweredCode;
  {
    llvm::raw_string_ostream os(loweredCode);
    moduleConverted->print(os, nullptr);
  }
  return loweredCode;
}

void mockBackend(QuantumTask quantumTask) {
  // RabbitMQServer forwardQueue(AMQP_SERVER, AMQP_PORT,
  //                             QUEUE_SCHEDULER_TRANSPILER,
  //                             AMQP_USER,
  //                             AMQP_PASSWORD);
  // json taskJson = dumpQuantumTaskToJson(quantumTask);
  //  the functionaliyt of the pass runner goes here!

  // after processing, move forward the quantum task
  // forwardQueue.publishMessage(taskJson.dump(), true);
  std::cout << "Received task with id: " << quantumTask.task_id << std::endl;
  for (auto task : quantumTask.circuit_files)
    std::cout << task << std::endl;
  // TODO
  // Extract job details from the request body
  // std::string jobName = quantumTask.circuit_name;
  int jobCount = quantumTask.n_shots;
  std::string program = quantumTask.circuit_files[0];
  // Simulate kernel function and qubit processing
  std::string kernelName = getKernelName(program);
  std::string qirCode = lowerQuakeCode(program, kernelName);
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
