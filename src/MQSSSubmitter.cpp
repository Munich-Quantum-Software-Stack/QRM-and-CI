

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

#define QDMI_CONF_PATH "cxx_qdmi.conf"

using namespace mqss;
struct CouplingTy {
  uint64_t src_id;
  uint64_t dst_id;
};

struct DeviceProperty {
  int numQubits = -223;
  std::vector<CouplingTy> cm;
};

// API to fetch number of Qubits of the target device
static int getDeviceNumQubits(QDMI_Device device) {

  // Step 1: get the size
  size_t size_ret = 0;
  int ret = -223;
  int numQubits;

  ret = QDMI_device_query_device_property(
      device, QDMI_DEVICE_PROPERTY_QUBITSNUM,
      sizeof(size_t), // ← size of the buffer you're providing
      &numQubits,     // ← pointer to receive the value directly
      nullptr         // ← size_ret not needed);
  );
  assert(ret == QDMI_SUCCESS);
  return numQubits;
}

// Fetching the coupling map of the target device
static std::vector<CouplingTy>
getDeviceCouplingMap(QDMI_Device device,
                     std::shared_ptr<spdlog::logger> MQSSLogger) {
  // Step 1: get the size
  size_t size_ret = 0;
  int ret = -223;
  ret = QDMI_device_query_device_property(
      device, QDMI_DEVICE_PROPERTY_COUPLINGMAP, 0, nullptr, &size_ret);

  MQSSLogger->info("-->Query coupling map size: ", size_ret);
  assert(ret == QDMI_SUCCESS);
  // size_ret = 20 * sizeof(QDMI_Site) for the cxx device
  size_t num_entries = size_ret / sizeof(QDMI_Site); // = 20
  size_t num_pairs = num_entries / 2;                // = 10

  // Step 2: retrieve
  std::vector<QDMI_Site> queired_coupling_map(num_entries);
  ret = QDMI_device_query_device_property(
      device, QDMI_DEVICE_PROPERTY_COUPLINGMAP, size_ret,
      static_cast<void *>(queired_coupling_map.data()), nullptr);

  MQSSLogger->info("-->Query coupling map entries: ",
                   queired_coupling_map.size());
  assert(ret == QDMI_SUCCESS);
  std::vector<CouplingTy> coupling_map_set;
  // Step 3: iterate over pairs
  for (size_t i = 0; i < num_entries; i += 2) {
    QDMI_Site src = queired_coupling_map[i];
    QDMI_Site dst = queired_coupling_map[i + 1];

    // query the index of each site
    uint64_t src_id = 0, dst_id = 0;
    QDMI_device_query_site_property(device, src, QDMI_SITE_PROPERTY_INDEX,
                                    sizeof(uint64_t), &src_id, nullptr);
    QDMI_device_query_site_property(device, dst, QDMI_SITE_PROPERTY_INDEX,
                                    sizeof(uint64_t), &dst_id, nullptr);

    CouplingTy coupling;
    coupling.src_id = src_id;
    coupling.dst_id = dst_id;
    coupling_map_set.push_back(coupling);
  }
  return coupling_map_set;
}

// Getting device properties such as No. of Qubits, Coupling Map etc.
// TODO: Need to enable this.
static DeviceProperty
getDeviceProperties(QDMI_Device Device,
                    std::shared_ptr<spdlog::logger> MQSSLogger) {

  DeviceProperty Properties;
  Properties.numQubits = getDeviceNumQubits(Device);
  Properties.cm = getDeviceCouplingMap(Device, std::move(MQSSLogger));

  return Properties;
}

// Test to check the complex state vector amplitudes received after executing
// the QDMI Job.
// TODO: Need a better check here!
::testing::AssertionResult
CheckBellState(const std::vector<std::complex<double>> &state_vector,
               std::shared_ptr<spdlog::logger> MQSSLogger, double tol = 1e-6) {
  if (!state_vector.empty() && state_vector.size() != 32) {
    MQSSLogger->error("Expected 32 amplitudes got: {}", state_vector.size());
    return testing::AssertionFailure();
  }
  constexpr double inv_sqrt2 = 0.70710678118654752440;
  const double a00 = std::abs(state_vector[0]);
  const double a01 = std::abs(state_vector[1]);
  const double a10 = std::abs(state_vector[2]);
  const double a11 = std::abs(state_vector[3]);

  MQSSLogger->info(
      "Results of Quantum Job are the following (First 4 amplitudes):");
  MQSSLogger->info("|00> amplitude: {}", a00);
  MQSSLogger->info("|01> amplitude: {}", a01);
  MQSSLogger->info("|10> amplitude: {}", a10);
  MQSSLogger->info("|11> amplitude: {}", a11);

  return ::testing::AssertionSuccess();
}

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
  ;
}

// Use QDMI API's to create a QDMI Job and submit it to the QDMI device.
static QuantumResult createAndSubmitQDMIJobToQDMIDevice(
    mqss::QuantumTask task, const std::string &libName,
    const std::string &prefix, std::shared_ptr<spdlog::logger> MQSSLogger) {

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

  MQSSLogger->info("Raw HIST KEYS size: {}", size);
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

  for (size_t i = 0; i < counts.size(); i++) {
    MQSSLogger->info("counts[{}]: {}", i, counts[i]);
  }

  QuantumResult result;
  result.set_task_id(task.task_id());

  result.add_executed_circuits(circuit);
  std::string return_String = "";
  for (auto entry : key_vec) {
    return_String += entry + " ";
  }
  result.set_additional_information(return_String);

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
    Logger::cleanup();
    exit(0);
  });

  mqss::Logger::init(SUBMITTER_LOG_FILES_PATH, LOGGER_SUBMITTER);
  auto logger = Logger::getLogger();
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
    std::string libName = "/workspaces/QRM/_deps/core/build/src/qdmi/devices/"
                          "dd/libmqt-core-qdmi-ddsim-device.so";
    logger->info("Device conf is: " + libName);

    auto circuit_result =
        createAndSubmitQDMIJobToQDMIDevice(task, libName, "MQT_DDSIM", logger);

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
