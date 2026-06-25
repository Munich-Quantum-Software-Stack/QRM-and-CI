

#include "ConnectionHandler.hpp"
#include "LoggerHandler.hpp"
#include "common/Logger.hpp"
#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/protocol/ProtoProtocol.hpp"
#include "mqss/transport/RabbitMqSimpleTransport.hpp"
#include "mqss/transport/Transport.hpp"
// #include "qdmi/client.h"
// #include "qdmi/constants.h"
// #include "qdmi/device.h"
#include "Driver.hpp"
// #include "qdmi_example_driver.h"
// #include "qdmi/constants.h"
#include "qinfo.h"

#include "gtest/gtest.h"
#include <cassert>
#include <complex>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>


static std::tuple<std::string, std::string> extractQDMIObj(std::string conf_path) {
  std::ifstream conf(conf_path);
  std::string line;

  while (std::getline(conf, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#')
      continue;

    std::istringstream iss(line);
    std::string path, prefix;

    if (iss >> path >> prefix) {
      return std::make_tuple(path, prefix);
    }
  }
}

// Load the device library dynamically
static std::pair<std::reference_wrapper<qdmi_main_driver::Driver>, QDMI_Device>
addDynamicDeviceLibrary(const std::string &libName, const std::string &prefix,
                        std::shared_ptr<spdlog::logger> MQSSLogger) {

  qdmi_main_driver::DeviceSessionConfig config;
  // If connecting to a remote device, use:
  // config.baseUrl = "http://localhost:8080";
  // config.token = "test_token";
  // config.authUrl = "https://auth.example.com";
  // config.username = "user";
  // config.password = "pass";
  // config.custom1 = "value1";
  // config.custom2 = "value2";
  // config.custom3 = "value3";
  // config.custom4 = "value4";
  // config.custom5 = "value5";

  auto &driver = qdmi_main_driver::Driver::get();

  auto *device = driver.addDynamicDeviceLibrary(libName, prefix, config);
  size_t namesSize = 0;
  size_t ret = 0;
  ret = QDMI_device_query_device_property(device, QDMI_DEVICE_PROPERTY_NAME, 0,
                                          nullptr, &namesSize);

  assert(ret == QDMI_SUCCESS);
  std::string name(namesSize - 1, '\0');
  ret = QDMI_device_query_device_property(device, QDMI_DEVICE_PROPERTY_NAME,
                                          namesSize, name.data(), nullptr);

  assert(ret == QDMI_SUCCESS);
  MQSSLogger->info("Device name: {} ", name);
  return std::make_pair(std::ref(driver), device);
}

// Use QDMI API's to create a QDMI Job and submit it to the QDMI device.
static mqss::QuantumResult
createAndSubmitQDMIJobToQDMIDevice(mqss::QuantumTask task,
                                   const std::string &device_conf_path,
                                   std::shared_ptr<spdlog::logger> MQSSLogger) {

  auto [libName, prefix] = extractQDMIObj(device_conf_path);

  auto [driver_ref, dev] = addDynamicDeviceLibrary(libName, prefix, MQSSLogger);
  QDMI_Job job = nullptr;
  int ret = 0;

  qdmi_main_driver::Driver &driver = driver_ref.get();
  QDMI_Session session = nullptr;
  driver.sessionAlloc(&session);

  auto circuit = task.circuit_files()[0];
  ret = QDMI_device_create_job(dev, &job);
  assert(ret == QDMI_SUCCESS);

  MQSSLogger->info("Created QDMI Job...");
  // Set Properties for the Job
  // Properties set:
  //    1. Circuit format (QIR-base profile or OPENQASM2)
  //    2. Circuit size and source string
  //    3. Number of shots
  const auto format = QDMI_PROGRAM_FORMAT_QASM2;
  ret = QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_PROGRAMFORMAT,
                               sizeof(QDMI_Program_Format), &format);

  assert(ret == QDMI_SUCCESS);

  int num_shots = task.n_shots();
  ret = QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_PROGRAM,
                               circuit.size() + 1, circuit.c_str());
  assert(ret == QDMI_SUCCESS);

  if (num_shots > 0) {
    ret = QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_SHOTSNUM,
                                 sizeof(size_t), &num_shots);
    assert(ret == QDMI_SUCCESS);
    MQSSLogger->info("--> QDMI Num shots: " + std::to_string(num_shots));
  }

  MQSSLogger->info("QDMI Job parameters Set...");
  // Submit the job and wait
  ret = QDMI_job_submit(job);
  assert(ret == QDMI_SUCCESS);

  MQSSLogger->info("QDMI Job submitted to QDMI Device...");
  ret = QDMI_job_wait(job, 0);
  if (ret != QDMI_SUCCESS) {
    MQSSLogger->error("QDMI job wait failed with: {}", ret);
  }

  assert(ret == QDMI_SUCCESS);

  QDMI_Job_Status status{};
  ret = QDMI_job_check(job, &status);

  assert(ret == QDMI_SUCCESS);
  MQSSLogger->info("QDMI Job status: {}", status);
  // Teardown (in reverse order)

  // Fetch Results of Job execution
  // TODO: What result to fetch (here: QDMI_JOB_RESULT_STATEVECTOR_DENSE)? This
  //        should be gathered from the user or quantumtask. There is another
  //        enum "QDMI_JOB_RESULT_CUSTOM" which is defined by the target device.
  //        Perhaps, that should be used for real device. However, the example
  //        qdmi device used for this test does not support custom job results.
  size_t size = 0;
  ret = QDMI_job_get_results(job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &size);
  assert(ret == QDMI_SUCCESS);
  // const size_t vec_length = state_size / sizeof(double);
  // assert(vec_length % 2 == 0);

  std::string key_list(size - 1, '\0');
  ret = QDMI_job_get_results(
      job, QDMI_JOB_RESULT_HIST_KEYS, size,
      static_cast<void *>(const_cast<char *>(key_list.data())), nullptr);

  assert(ret == QDMI_STATUS::QDMI_SUCCESS);

  std::vector<std::string> key_vec;
  std::string token;
  std::stringstream ss(key_list);
  while (std::getline(ss, token, ',')) {
    key_vec.emplace_back(token);
  }

  size_t val_size = 0;
  ret = QDMI_job_get_results(job, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr,
                             &val_size);
  assert(ret == QDMI_SUCCESS);

  auto type = val_size / sizeof(size_t);

  std::vector<size_t> counts(type);
  ret = QDMI_job_get_results(job, QDMI_JOB_RESULT_HIST_VALUES, val_size,
                             static_cast<void *>(counts.data()), nullptr);
  assert(ret == QDMI_SUCCESS);

  mqss::QuantumResult result;
  result.set_task_id(task.task_id());

  result.add_executed_circuits(circuit);

  result.set_execution_status(0);

  auto *res = result.add_results();
  if (key_vec.size() != counts.size()) {
    MQSSLogger->error("Size of Keys Vector not equal to Number of Counts!");
  };

  for (unsigned i = 0; i < key_vec.size(); ++i) {
    (*res->mutable_counts())[key_vec[i]] = counts[i];
  }

  QDMI_job_free(job);
  // QDMI_session_free(session);
  driver.sessionFree(session);

  return result;
}

int main() {

  std::signal(SIGINT, [](int) {
    mqss::Logger::cleanup();
    exit(0);
  });

  std::signal(SIGTERM, [](int) {
    mqss::Logger::cleanup();
    exit(0);
  });

  mqss::Logger::init(SUBMITTER_LOG_FILES_PATH, LOGGER_SUBMITTER);
  auto logger = mqss::Logger::getLogger();
  logger->info("Running up the MQSS Submitter");

  mqss::TransportOptions<mqss::RabbitMqSimple> opts;
  opts.host = AMQP_SERVER;
  opts.port = AMQP_PORT;
  opts.username = AMQP_USER;
  opts.password = AMQP_PASSWORD;

  mqss::Messenger<mqss::RabbitMqSimple, mqss::ProtoJson> messenger(opts);

  // Poll for incoming tasks
  while (true) {
    logger->info("Waiting for a new job...");
    auto res = messenger.receive<mqss::QuantumTask>(
        {SUBMITTER_QUEUE}, mqss::ReceiveArgs{
                               .timeout = std::chrono::milliseconds(5000),
                               .ack_mode = mqss::AckMode::Auto,
                           });

    if (!res.has_value()) {
      // Timeout is normal — just keep polling
      if (res.error().code() == mqss::StatusCode::Timeout) {
        continue;
      }
      logger->error("Receive error: ", res.error().reason());
      continue;
    }

    mqss::QuantumTask &task = *res;
    logger->info("Processing new task with id: {}", task.task_id());

    // Debugging: Print Circuit files received (qasm or qir)...
    // logger->info("-->Decoded task id: {}", task.task_id());
    // logger->info("-->New Circuit files dump:\n");
    // auto decoded_circuits = task.circuit_files();
    // for (auto circuit : decoded_circuits) {
    //   logger->info(circuit);
    // }

    // prepare QDMI job and submit
    // Set the path to the QDMI Device Shared Object file

    std::string device_prefix = "/workspaces/QRM/qdmi_device_objs/mqt_qdmi.conf";
    logger->info("Device conf is: " + device_prefix);

    auto circuit_result = createAndSubmitQDMIJobToQDMIDevice(
        task, device_prefix, logger);

    // auto circuit_result =
    //     createAndSubmitQDMIJob(task, device_conf.c_str(), std::move(logger));

    auto send_st = messenger.send<mqss::QuantumResult>(
        {task.result_destination()}, // use the queue the daemon specified
        circuit_result);

    // After send
    logger->info("Result sent by the Submitter for task: {}", task.task_id());
    if (!send_st.ok())
      logger->error("Failed to send result: {}", send_st.reason());
  }

  return 0;
}
