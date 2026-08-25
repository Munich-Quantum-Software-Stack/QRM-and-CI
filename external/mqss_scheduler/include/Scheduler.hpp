/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace mqss {

template <typename T>
concept Schedulable = requires(T job) {
  { job.task_id() } -> std::convertible_to<uint64_t>;
  { job.priority() } -> std::convertible_to<int>;
};

enum class SchedulingPolicy : std::uint8_t { FirstInFirstOut, PriorityBased };

template <Schedulable JobType> class Scheduler {
public:
  explicit Scheduler(
      SchedulingPolicy policy = SchedulingPolicy::FirstInFirstOut)
      : currentPolicy(policy) {}

  // Schedule a single job based on the current scheduling policy
  void scheduleJob(const JobType &job) {
    std::lock_guard<std::mutex> lock(mutexJobQueue);
    if (currentPolicy == SchedulingPolicy::FirstInFirstOut) {
      jobQueue.push_back(job);
    } else if (currentPolicy == SchedulingPolicy::PriorityBased) {
      auto it = std::find_if(jobQueue.begin(), jobQueue.end(),
                             [&](const JobType &existingJob) {
                               return job.priority() > existingJob.priority();
                             });
      jobQueue.insert(it, job);
    }
  }

  // Schedule multiple jobs at once
  void scheduleJobs(const std::vector<JobType> &jobs) {
    for (const auto &job : jobs) {
      scheduleJob(job);
    }
  }

  // Function to clear all jobs in the queue
  void clearJobs() {
    std::lock_guard<std::mutex> lock(mutexJobQueue);
    jobQueue.clear();
  }

  // Function to get the current number of jobs in the queue
  size_t getJobCount() const {
    std::lock_guard<std::mutex> lock(mutexJobQueue);
    return jobQueue.size();
  }

  // Function to get the current scheduling policy
  SchedulingPolicy getSchedulingPolicy() const { return currentPolicy; }

  // Function to get the next ready job based on the current scheduling policy
  std::optional<JobType> getNextReadyJob() {
    std::lock_guard<std::mutex> lock(mutexJobQueue);
    if (jobQueue.empty()) {
      return std::nullopt;
    }
    JobType nextJob = jobQueue.front();
    jobQueue.erase(jobQueue.begin());
    return nextJob;
  }

private:
  std::vector<JobType> jobQueue;
  SchedulingPolicy currentPolicy;
  mutable std::mutex mutexJobQueue; // Mutex to protect access to the job queue
};

} // namespace mqss
