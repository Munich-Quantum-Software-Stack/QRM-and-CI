

#include "ConnectionHandler.hpp"
#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/protocol/ProtoProtocol.hpp"
#include "mqss/transport/RabbitMqSimpleTransport.hpp"
#include "mqss/transport/Transport.hpp"
#include "qdmi/client.h"
#include "qdmi/constants.h"
#include "qdmi/device.h"
#include "qdmi_example_driver.h"
#include "qinfo.h"

#include <cassert>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "common/Logger.hpp"
#include "LoggerHandler.hpp"
#include <csignal>


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

static DeviceProperty
getDeviceProperties(QDMI_Device Device,
                    std::shared_ptr<spdlog::logger> MQSSLogger) {

  DeviceProperty Properties;
  Properties.numQubits = getDeviceNumQubits(Device);
  Properties.cm = getDeviceCouplingMap(Device, std::move(MQSSLogger));

  return Properties;
}

static QuantumResult createAndSubmitQDMIJob(mqss::QuantumTask task,
                                  const char *device_conf_path,
                                  std::shared_ptr<spdlog::logger> MQSSLogger) {

  QDMI_Job job = nullptr;
  int num_shots = task.n_shots();
  auto circuit = task.circuit_files()[0];

  setenv("QDMI_CONF", device_conf_path, 1);

  int ret = QDMI_driver_init();
  assert(ret == QDMI_SUCCESS);

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
  assert(ret == QDMI_SUCCESS);

  MQSSLogger->info("Initialized new QDMI session...");
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

  MQSSLogger->info("--> QDMI Num Devices: " + std::to_string(devices.size()));
  assert(ret == QDMI_SUCCESS);

  MQSSLogger->info("Found QDMI Device...");
  // Create a Job for the QDMI device
  // TODO: What if there are more than 1 device to target?
  QDMI_Device dev = devices[0];

  ret = QDMI_device_create_job(dev, &job);
  assert(ret == QDMI_SUCCESS);

  MQSSLogger->info("Created QDMI Job...");
  // Set Properties for the Job
  // Properties set:
  //    1. Circuit format (QIR-base profile)
  //    2. Circuit size and source string
  //    3. Number of shots
  const auto format = QDMI_PROGRAM_FORMAT_QIRBASESTRING;
  ret = QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_PROGRAMFORMAT,
                               sizeof(QDMI_Program_Format), &format);

  assert(ret == QDMI_SUCCESS);

  ret = QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_PROGRAM,
                               circuit.size() + 1, circuit.c_str());
  assert(ret == QDMI_SUCCESS);

  if (num_shots > 0) {
    ret = QDMI_job_set_parameter(job, QDMI_JOB_PARAMETER_SHOTSNUM,
                                 sizeof(size_t), &num_shots);
    assert(ret == QDMI_SUCCESS);
  }

  MQSSLogger->info("QDMI Job parameters Set...");
  // Submit the job and wait
  ret = QDMI_job_submit(job);
  assert(ret == QDMI_SUCCESS);

  MQSSLogger->info("QDMI Job submitted to QDMI Device...");
  ret = QDMI_job_wait(job, 0);
  
  assert(ret == QDMI_SUCCESS);
  
  QDMI_Job_Status status{};
  ret = QDMI_job_check(job, &status);

  assert(ret == QDMI_SUCCESS);
  MQSSLogger->info("QDMI Job status: ", status);
  // Teardown (in reverse order)

  // Fetch Results of Job execution
  // TODO: What result to fetch (here: QDMI_JOB_RESULT_STATEVECTOR_DENSE) should
  // come
  //        from the user or quantumtask. There is another enum
  //        "QDMI_JOB_RESULT_CUSTOM" which is defined by the target device.
  //        Perhaps, that should be used for real device. However, the example
  //        qdmi device used for this test does not support custom job results.
  size_t state_size = 0;
  ret = QDMI_job_get_results(job, QDMI_JOB_RESULT_STATEVECTOR_DENSE, 0, nullptr,
                             &state_size);
  assert(ret == QDMI_SUCCESS);
  const size_t vec_length = state_size / sizeof(double);
  assert(vec_length % 2 == 0);

  std::vector<double> state_vector(vec_length);
  ret = QDMI_job_get_results(job, QDMI_JOB_RESULT_STATEVECTOR_DENSE, state_size,
                             state_vector.data(), nullptr);
  assert(ret == QDMI_SUCCESS);

  std::vector<std::complex<double>> complex_state_vector;
  complex_state_vector.reserve(vec_length / 2);
  for (size_t i = 0; i < state_vector.size(); i += 2) {
    complex_state_vector.emplace_back(state_vector[i], state_vector[i + 1]);
  }

  // assert that the complex vector is normalized up to a certain tolerance
  double norm = 0;
  for (const auto &val : complex_state_vector) {
    norm += std::norm(val);
  }

  MQSSLogger->info("Results of Job: ", norm);
  QuantumResult result;
  result.set_task_id(task.task_id());
  result.add_executed_circuits(circuit);
  result.set_additional_information(std::to_string(norm));

  QDMI_job_free(job);
  QDMI_session_free(session);
  QDMI_driver_shutdown(); // dlclose() happens here

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
    std::string device_name = "cxx";
    std::string device_conf = device_name + "_qdmi.conf";
    logger->info("Device conf is: " + device_conf);
    auto circuit_result =
        createAndSubmitQDMIJob(task, device_conf.c_str(), std::move(logger));

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
