/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <algorithm>
#include <concepts>
#include <mutex>
#include <optional>
#include <vector>

namespace mqss {

template <typename T>
concept Schedulable = requires(T job) {
  { job.task_id() } -> std::convertible_to<uint64_t>;
  { job.priority() } -> std::convertible_to<int>;
};

enum class SchedulingPolicy { FirstInFirstOut, PriorityBased };

template <Schedulable JobType> class Scheduler {
public:
  explicit Scheduler(
      SchedulingPolicy policy = SchedulingPolicy::FirstInFirstOut)
      : m_currentPolicy(policy) {}

  // Schedule a single job based on the current scheduling policy
  void scheduleJob(const JobType &job) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentPolicy == SchedulingPolicy::FirstInFirstOut) {
      m_jobQueue.push_back(job);
    } else if (m_currentPolicy == SchedulingPolicy::PriorityBased) {
      auto it = std::find_if(m_jobQueue.begin(), m_jobQueue.end(),
                             [&](const JobType &existingJob) {
                               return job.priority() > existingJob.priority();
                             });
      m_jobQueue.insert(it, job);
    }
  }

  // Schedule multiple jobs at once
  void scheduleJobs(const std::vector<JobType> &jobs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &job : jobs) {
      scheduleJob(job);
    }
  }

  // Function to clear all jobs in the queue
  void clearJobs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_jobQueue.clear();
  }

  // Function to get the current number of jobs in the queue
  size_t getJobCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jobQueue.size();
  }

  // Function to get the current scheduling policy
  SchedulingPolicy getSchedulingPolicy() const { return m_currentPolicy; }

  // Function to get the next ready job based on the current scheduling policy
  std::optional<JobType> getNextReadyJob() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_jobQueue.empty()) {
      return std::nullopt;
    }
    JobType nextJob = m_jobQueue.front();
    m_jobQueue.erase(m_jobQueue.begin());
    return nextJob;
  }

private:
  std::vector<JobType> m_jobQueue;
  SchedulingPolicy m_currentPolicy;
  mutable std::mutex m_mutex; // Mutex to protect access to the job queue
};

} // namespace mqss
