/*------------------------------------------------------------------------------
Copyright 2024 - 2026 Munich Quantum Software Stack
All rights reserved.

Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

https://llvm.org/LICENSE.txt

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
------------------------------------------------------------------------------*/

#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <mutex>
#include <optional>
#include <vector>

namespace mqss {

/**
 * @brief Minimal shape a task must expose to be schedulable.
 *
 * Following the MQSS QRM design, the concept of Schedulable is defined
 * in which quantum tasks must be schedulable objects. It can be usable
 * here with minimal modification.
 */
template <typename T>
concept Schedulable = requires(T task) {
    { task.task_id() } -> std::convertible_to<uint64_t>;
    { task.priority() } -> std::convertible_to<int>;
};

/**
 * @brief Schedulable concept with a qubit-count accessor/operand.
 *
 * This might be usefull for the Backfilling policy and only when
 * a concrete TaskType happens to provide it (checked via
 * `if constexpr`). Protobuf's QuantumTask satisfies this too
 * (its `n_qbits` field generates an `n_qbits()` getter).
 */
template <typename T>
concept SizedSchedulable = Schedulable<T> && requires(T task) {
    { task.n_qbits() } -> std::convertible_to<int>;
};

/**
 * @brief Available scheduling policies.
 *
 * Be able to add more scheduling policies in the future. For now,
 * FirstInFirstOut/PriorityBased are basic, RoundRobin/Backfilling/MixNMulti
 * might be usefull for the quantum task scheduling. The policies are designed to be
 * compatible with the MQSS QRM design, and can be used in the future
 * depending on the requirements.
 */
enum class SchedulingPolicy {
    FirstInFirstOut,
    PriorityBased,
    RoundRobin,
    Backfilling,
    MixNMulti
};

/**
 * @brief A standalone, dependency-free quantum task scheduler.
 *
 * Based on the semantics and constructor signature as MQSS QRM structure.
 * The method names here consistently say "Task" instead of "Job" because we
 * consider job has a more general view from HPC job. In each job, there are
 * classical/HPC computation tasks and quantum computation tasks.
 *
 * The scheduling policy is fixed at construction, matching QRM's own design
 * (no setPolicy - changing policy mid-flight would leave the internal
 * ordering inconsistent with whichever policy scheduled the tasks already
 * queued).
 */
template <Schedulable TaskType> class Scheduler {
public:
    explicit Scheduler(SchedulingPolicy policy = SchedulingPolicy::FirstInFirstOut);

    /// Schedule a single task according to the current policy.
    void scheduleTask(const TaskType &task);

    /// Schedule multiple tasks at once.
    void scheduleTasks(const std::vector<TaskType> &tasks);

    /// Remove all tasks currently in the queue.
    void clearTasks();

    /// Number of tasks currently queued.
    size_t getTaskCount() const;

    /// The policy this scheduler was constructed with.
    SchedulingPolicy getSchedulingPolicy() const;

    /// Pop and select the next task to run, or std::nullopt if none is ready.
    std::optional<TaskType> getNextReadyTask();

    /// Number of round-robin lanes in the case of 2 job submission pathways,
    /// e.g., HPC and cloud, where each lane is a FIFO queue and the scheduler
    /// can round-robin between them, Default: 2 lanes.
    void setNumLanes(size_t lanes);

    /// Capacity (in qubits) available to admit a queued task
    /// out of order. A negative value (the default) disables the capacity
    /// check, which might be usefull for Backfilling, otherwise it behaves
    /// like FirstInFirstOut.
    void setAvailableQubits(int qubits);

    /// A waiting time offset priority to set the effective priority of a task.
    /// The effective priority is calculated as tick of queue age adds `weight`
    /// to a task's effective priority, so a low-priority task that has waited
    /// long enough still eventually wins over a freshly-arrived high-priority
    /// one, default: 1.0.
    void setAgingWeight(double weight);

private:
    /// A queued task plus its arrival sequence number.
    struct Entry {
        TaskType task;
        uint64_t sequence;
    };

    // The callers scheduleTask/scheduleTasks methods take the mutex
    // and then call this helper. Split out so scheduleTasks can lock
    // once for the whole batch instead of once per task.
    void scheduleTaskLocked(const TaskType &task);

    std::optional<TaskType> nextFIFOPriorLocked(); // FirstInFirstOut, PriorityBased
    std::optional<TaskType> nextRoundRobinLocked(); // RoundRobin in the case of 2 job submission pathways, e.g., HPC and cloud, where each lane is a FIFO queue 
    // and the scheduler can round-robin between them.
    std::optional<TaskType> nextBackfillingLocked();
    std::optional<TaskType> nextMixNMultiLocked();

    // The queue of tasks, in arrival order, plus their sequence numbers.
    std::vector<Entry> taskQueue;
    SchedulingPolicy currentPolicy;
    mutable std::mutex mutex;

    uint64_t nextSequence = 0;

    size_t numLanes = 2;
    size_t laneCursor = 0;

    int availableQubits = -1;

    double agingWeight = 1.0;
};

} // namespace mqss

#include "scheduler.tpp"

#endif // SCHEDULER_HPP
