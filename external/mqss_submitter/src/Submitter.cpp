/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "Submitter.hpp"

mqss::Submitter::Submitter(const std::string &driver_name,
                           const std::string &device_name,
                           const std::string &token)
    : session_(driver_name, token, "", "", "", "", ""), device_(nullptr),
      device_available_(false) {
  // if device name is empty, select the first available device
  if (device_name.empty() and session_.get_devices().size() > 0) {
    device_ = session_.get_devices()[0];
    device_available_ = true;
  } else {
    for (const auto &device : session_.get_devices()) {
      std::cout << "Available device: " << device.get_name() << std::endl;
      if (device.get_name() == device_name) {
        device_ = device;
        device_available_ = true;
        break;
      }
    }
  }
  if (!device_available_) {
    throw std::runtime_error("Device not found: " + device_name);
  }
}

std::vector<std::optional<std::map<std::string, size_t>>>
mqss::Submitter::submitTask(const std::vector<std::string> &progs, size_t shots,
                            QDMI_Program_Format format) {
  if (!device_available_) {
    throw std::runtime_error("No device available for submission.");
  }
  // Submit the quantum task to the backend device
  // QDMI_PROGRAM_FORMAT_QIRBASESTRING = 2
  // Ensure the format is QIRBaseString for now if not throw an error
  if (format != QDMI_PROGRAM_FORMAT_QIRBASESTRING) {
    throw std::runtime_error("Unsupported program format. Only QIRBaseString "
                             "(format=2) is supported.");
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
  std::vector<std::optional<std::map<std::string, size_t>>> results_histogram;
  for (const auto &prog : progs) {
    auto job = device_.create_job(format, prog, shots);
    job.submit();
    job.wait();
    if (job.status() == 4) {
      if (job.results(QDMI_JOB_RESULT_HIST_KEYS).has_value()) {
        results_histogram.push_back(std::get<std::map<std::string, size_t>>(
            job.results(QDMI_JOB_RESULT_HIST_KEYS).value()));
      } else {
        results_histogram.push_back(std::nullopt);
      }
    } else {
      results_histogram.push_back(std::nullopt);
    }
  }
  return results_histogram;
}
