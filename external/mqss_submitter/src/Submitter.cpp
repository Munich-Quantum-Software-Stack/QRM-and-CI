/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "Submitter.h"

#include "qdmi/constants.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

mqss::submitter::Submitter::Submitter(const std::string &driverName,
                                      const std::string &deviceName,
                                      const std::string &deviceID,
                                      const std::string &token)
    : session(driverName, token, "", "", "", "", ""), device(nullptr),
      devID(deviceID) {
  // if device name is empty, select the first available device
  if (deviceName.empty() and session.get_devices().size() > 0) {
    device = session.get_devices()[0];
    initialized = true;
  } else {
    for (const auto &iDevice : session.get_devices()) {
      std::cout << "Available device: " << iDevice.get_name() << "\n";
      if (iDevice.get_name() == deviceName) {
        device = iDevice;
        initialized = true;
        break;
      }
    }
  }
  if (!initialized) {
    throw std::runtime_error("Device not found: " + deviceName);
  }
}

std::vector<std::optional<std::map<std::string, size_t>>>
mqss::submitter::Submitter::executeJob(const std::vector<std::string> &progs,
                                       size_t shots,
                                       QDMI_Program_Format format) {
  return getJobResultHistogram(this->submitJob(progs, shots, format))
      .value_or(std::vector<std::optional<std::map<std::string, size_t>>>());
  ;
}

const std::string &mqss::submitter::Submitter::getDeviceID() { return devID; }
size_t mqss::submitter::Submitter::getDeviceNumQubits() {
  return initialized ? device.get_num_qubits() : 0;
}
QDMI_Device_Status mqss::submitter::Submitter::getDeviceStatus() {
  return initialized ? static_cast<QDMI_Device_Status>(device.get_status())
                     : QDMI_DEVICE_STATUS_OFFLINE;
}
std::vector<std::string> mqss::submitter::Submitter::getDeviceInstructions() {
  std::vector<std::string> instructions;
  for (const auto &op : device.get_operations()) {
    instructions.push_back(op.get_name());
  }
  return instructions;
}
std::vector<std::pair<size_t, size_t>>
mqss::submitter::Submitter::getDeviceConnectivity() {
  std::vector<std::pair<size_t, size_t>> connectivity;
  for (const auto &pair : device.get_coupling_map()) {
    connectivity.emplace_back(pair.first.get_id(), pair.second.get_id());
  }
  return connectivity;
}
std::vector<QDMI_Program_Format>
mqss::submitter::Submitter::getDeviceSupportedCircuitFormats() {
  std::vector<QDMI_Program_Format> supportedFormats;
  for (const auto &format : device.get_supported_program_formats()) {
    supportedFormats.push_back(format);
  }
  return supportedFormats;
}

std::uint32_t
mqss::submitter::Submitter::submitJob(const std::vector<std::string> &progs,
                                      size_t shots,
                                      QDMI_Program_Format format) {
  if (!initialized) {
    throw std::runtime_error("No device available for submission.");
  }
  // Submit the quantum task to the backend device
  // QDMI_PROGRAM_FORMAT_QIRBASESTRING = 2
  // Ensure the format is QIRBaseString for now if not throw an error
  if (format != QDMI_PROGRAM_FORMAT_QIRBASESTRING) {
    throw std::runtime_error("Unsupported program format. Only QIRBaseString "
                             "(format=2) is supported.");
  }
  // For simplicity, we will only submit the first program in the list.
  if (progs.empty()) {
    throw std::runtime_error("No programs provided for submission.");
  }
  std::vector<qdmi::Job> jobs;
  for (const auto &prog : progs) {
    auto job = device.create_job(format, prog, shots);
    job.submit();
    jobs.push_back(std::move(job));
  }

  activeJobs[nextJobId] = Job(std::move(jobs));
  return nextJobId++;
}

std::optional<bool>
mqss::submitter::Submitter::isJobFinished(std::uint32_t jobId) {
  auto it = activeJobs.find(jobId);
  if (it == activeJobs.end()) {
    return std::nullopt; // Return nullopt if the job ID is not found
  }
  return std::make_optional(it->second.isFinished());
}

std::optional<std::vector<std::optional<std::map<std::string, size_t>>>>
mqss::submitter::Submitter::getJobResultHistogram(std::uint32_t jobId) {
  auto it = activeJobs.find(jobId);
  if (it == activeJobs.end()) {
    return std::nullopt; // Return nullopt if the job ID is not found
  }
  auto histogram = it->second.getHistogram();
  activeJobs.erase(
      it); // Remove the job from activeJobs after retrieving results
  return std::make_optional(histogram);
}

/*
QDMI_JOB_STATUS_CREATED = 0,
/// The job was submitted.
QDMI_JOB_STATUS_SUBMITTED = 1,
/// The job was received, and is waiting to be executed.
QDMI_JOB_STATUS_QUEUED = 2,
/// The job is running, and the result is not yet available.
QDMI_JOB_STATUS_RUNNING = 3,
/// The job is done, and the result can be retrieved.
QDMI_JOB_STATUS_DONE = 4,
/// The job was canceled, and the result is not available.
QDMI_JOB_STATUS_CANCELED = 5,
/// An error occurred in the job's lifecycle.
QDMI_JOB_STATUS_FAILED = 6
  */
/*
  QDMI_JOB_RESULT_SHOTS = 0,
  QDMI_JOB_RESULT_HIST_KEYS = 1,
  QDMI_JOB_RESULT_HIST_VALUES = 2,
  QDMI_JOB_RESULT_STATEVECTOR_DENSE = 3,
  QDMI_JOB_RESULT_PROBABILITIES_DENSE = 4,
  QDMI_JOB_RESULT_STATEVECTOR_SPARSE_KEYS = 5,
  QDMI_JOB_RESULT_STATEVECTOR_SPARSE_VALUES = 6,
  QDMI_JOB_RESULT_PROBABILITIES_SPARSE_KEYS = 7,
  QDMI_JOB_RESULT_PROBABILITIES_SPARSE_VALUES = 8,
*/
std::vector<std::optional<std::map<std::string, size_t>>>
mqss::submitter::Job::getHistogram() {
  wait();
  std::vector<std::optional<std::map<std::string, size_t>>> resultsHistogram;
  for (auto &job : jobs) {
    if (job.status() == QDMI_JOB_STATUS_DONE ||
        job.status() == QDMI_JOB_STATUS_FAILED ||
        job.status() == QDMI_JOB_STATUS_CANCELED) {
      if (job.results(QDMI_JOB_RESULT_HIST_KEYS).has_value()) {
        resultsHistogram.emplace_back(std::get<std::map<std::string, size_t>>(
            job.results(QDMI_JOB_RESULT_HIST_KEYS).value()));
      } else {
        resultsHistogram.emplace_back(std::nullopt);
      }
    } else {
      resultsHistogram.emplace_back(std::nullopt);
    }
  }
  return resultsHistogram;
}

bool mqss::submitter::Job::isFinished() {
  return std::ranges::all_of(jobs, [](qdmi::Job &job) {
    return job.status() == QDMI_JOB_STATUS_DONE ||
           job.status() == QDMI_JOB_STATUS_FAILED ||
           job.status() == QDMI_JOB_STATUS_CANCELED;
  });
}
