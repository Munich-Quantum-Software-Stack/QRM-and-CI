/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "qdmi.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mqss::submitter {

class Job;

class Submitter {
public:
  Submitter() = delete; // Delete default constructor to enforce initialization
                        // with parameters

  // Constructor to initialize the Submitter with the backend driver, device
  // name, device ID, and authentication token
  Submitter(const std::string &driverName, const std::string &deviceName,
            const std::string &deviceID, const std::string &token);

  // Function to execute a quantum job on the backend
  // @param progs A vector of quantum programs to execute
  // @param shots The number of shots to execute for each program
  // @param format The format of the quantum programs (default is QIRBASESTRING)
  // @return A vector of optional histograms for each program executed
  std::vector<std::optional<std::map<std::string, size_t>>>
  executeJob(const std::vector<std::string> &progs, size_t shots,
             QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QIRBASESTRING);

  // Function to submit a quantum job to the backend
  // @param progs A vector of quantum programs to submit
  // @param shots The number of shots to execute for each program
  // @param format The format of the quantum programs (default is QIRBASESTRING)
  // @return The job ID assigned to the submitted job
  std::uint32_t
  submitJob(const std::vector<std::string> &progs, size_t shots,
            QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QIRBASESTRING);

  // Function to check if a submitted job has finished executing
  // @param jobId The ID of the job to check
  // @return An optional boolean indicating if the job has finished (true),
  //         is still running (false), or if the job ID is not found (nullopt)
  std::optional<bool> isJobFinished(std::uint32_t jobId);

  // Function to retrieve the result histogram of a submitted job
  // @param jobId The ID of the job to retrieve results for
  // @return An optional vector of optional histograms for each program in the
  //         job, or nullopt if the job ID is not found
  std::optional<std::vector<std::optional<std::map<std::string, size_t>>>>
  getJobResultHistogram(std::uint32_t jobId);

  // Function to get the number of pending jobs (jobs that are either queued or
  // currently executing)
  // @return The number of jobs currently in the queue or being processed
  size_t getNumPendingJobs() const { return activeJobs.size(); }

  // Accessor methods to retrieve device information
  // @return The device ID of the backend
  const std::string &getDeviceID();

  // @return The number of qubits available on the backend
  size_t getDeviceNumQubits();

  // @return The current status of the backend device
  QDMI_Device_Status getDeviceStatus();

  // @return A vector of supported quantum instructions for the backend
  std::vector<std::string> getDeviceInstructions();

  // @return A vector of qubit connectivity pairs for the backend
  std::vector<std::pair<size_t, size_t>> getDeviceConnectivity();

  // @return A vector of supported circuit formats for the backend
  std::vector<QDMI_Program_Format> getDeviceSupportedCircuitFormats();

private:
  std::unordered_map<std::uint32_t, Job> activeJobs;
  qdmi::Session session;
  qdmi::Device device;
  std::string devID;
  std::uint32_t nextJobId{1};
  bool initialized{false};
};

class Job {

public:
  Job() = default;

  explicit Job(std::vector<qdmi::Job> jobs) : jobs{std::move(jobs)} {}
  explicit Job(qdmi::Job job) { jobs.push_back(std::move(job)); }

  void addJob(qdmi::Job job) { jobs.push_back(std::move(job)); }

  bool isFinished();

  void wait() {
    for (auto &job : jobs) {
      job.wait();
    }
  }

  std::vector<std::optional<std::map<std::string, size_t>>> getHistogram();

private:
  std::vector<qdmi::Job> jobs;
};

} // namespace mqss::submitter
