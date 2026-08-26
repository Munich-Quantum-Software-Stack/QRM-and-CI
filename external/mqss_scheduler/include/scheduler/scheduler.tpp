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

// Template for the quantum task scheduler, split out of scheduler.hpp;
// kept header-visible - rather than moved into scheduler.cpp as ordinary
// out-of-line definitions. This might be clear for Scheduler as it must stay
// instantiable for any TaskType a *consumer* project chooses (e.g. QRM's protobuf
// mqss::QuantumTask).
// scheduler.cpp only explicitly instantiates it for this repo's own
// QuantumTask type, as a concrete, compiled example/test target.

#ifndef SCHEDULER_TPP
#define SCHEDULER_TPP

#include <algorithm>

namespace mqss {

template <Schedulable TaskType>
Scheduler<TaskType>::Scheduler(SchedulingPolicy policy) : currentPolicy(policy) {}

template <Schedulable TaskType>
void Scheduler<TaskType>::scheduleTaskLocked(const TaskType &task) {
    if (currentPolicy == SchedulingPolicy::PriorityBased) {
        // Insert the new task into the queue in priority order (higher priority first).
        auto it = std::find_if(taskQueue.begin(), taskQueue.end(),
                                [&](const Entry &existing) { return task.priority() > existing.task.priority(); });
        taskQueue.insert(it, Entry{task, nextSequence++});
    } else {
        // FirstInFirstOut, RoundRobin, Backfilling, and MixNMulti all keep
        // arrival order in the underlying queue; when getNextReadyTask() is called,
        // the scheduler will apply the appropriate policy to select which task
        // to return. The queue is always kept in arrival order.
        taskQueue.push_back(Entry{task, nextSequence++});
    }
}

template <Schedulable TaskType> void Scheduler<TaskType>::scheduleTask(const TaskType &task) {
    std::lock_guard<std::mutex> lock(mutex);
    scheduleTaskLocked(task);
}

template <Schedulable TaskType>
void Scheduler<TaskType>::scheduleTasks(const std::vector<TaskType> &tasks) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto &task : tasks) {
        scheduleTaskLocked(task);
    }
}

template <Schedulable TaskType> void Scheduler<TaskType>::clearTasks() {
    std::lock_guard<std::mutex> lock(mutex);
    // Clear the task queue and related pointers.
    taskQueue.clear();
    laneCursor = 0;
}

template <Schedulable TaskType> size_t Scheduler<TaskType>::getTaskCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    // Return the number of tasks currently in the queue.
    return taskQueue.size();
}

template <Schedulable TaskType> SchedulingPolicy Scheduler<TaskType>::getSchedulingPolicy() const {
    // Return the scheduling policy this scheduler was constructed with.
    return currentPolicy;
}

template <Schedulable TaskType> void Scheduler<TaskType>::setNumLanes(size_t lanes) {
    std::lock_guard<std::mutex> lock(mutex);
    // Ensure at least one lane is used; if lanes is 0, default to 1.
    // This works for RoundRobin.
    numLanes = lanes == 0 ? 1 : lanes;
    laneCursor = 0;
}

template <Schedulable TaskType> void Scheduler<TaskType>::setAvailableQubits(int qubits) {
    std::lock_guard<std::mutex> lock(mutex);
    // Set the available qubits for Backfilling. A negative value disables the capacity check.
    availableQubits = qubits;
}

template <Schedulable TaskType> void Scheduler<TaskType>::setAgingWeight(double weight) {
    std::lock_guard<std::mutex> lock(mutex);
    // Set the aging weight for how waiting time offsets priority.
    agingWeight = weight;
}

template <Schedulable TaskType> std::optional<TaskType> Scheduler<TaskType>::nextFIFOPriorLocked() {
    if (taskQueue.empty()) {
        return std::nullopt;
    }
    // For FirstInFirstOut and PriorityBased, the next task is simply the first in the queue.
    TaskType task = taskQueue.front().task;
    taskQueue.erase(taskQueue.begin());
    return task;
}

template <Schedulable TaskType> std::optional<TaskType> Scheduler<TaskType>::nextRoundRobinLocked() {
    if (taskQueue.empty()) {
        return std::nullopt;
    }
    // Try each lane once, starting at the cursor, so a lane with nothing
    // waiting doesn't stall dispatch - it's just skipped this round.
    for (size_t attempt = 0; attempt < numLanes; ++attempt) {
        const size_t lane = (laneCursor + attempt) % numLanes;
        auto it = std::min_element(
            taskQueue.begin(), taskQueue.end(), [&](const Entry &a, const Entry &b) {
                const bool aInLane = static_cast<uint64_t>(a.task.task_id()) % numLanes == lane;
                const bool bInLane = static_cast<uint64_t>(b.task.task_id()) % numLanes == lane;
                if (aInLane != bInLane) {
                    return aInLane; // entries in this lane sort before all others
                }
                return a.sequence < b.sequence; // FIFO within the lane
            });
        if (it != taskQueue.end() && static_cast<uint64_t>(it->task.task_id()) % numLanes == lane) {
            TaskType task = it->task;
            taskQueue.erase(it);
            laneCursor = (lane + 1) % numLanes;
            return task;
        }
    }
    return std::nullopt;
}

template <Schedulable TaskType> std::optional<TaskType> Scheduler<TaskType>::nextBackfillingLocked() {
    if (taskQueue.empty()) {
        return std::nullopt;
    }
    if (availableQubits < 0) {
        // No capacity, then behave like FIFO.
        return nextFIFOPriorLocked();
    }
    for (auto it = taskQueue.begin(); it != taskQueue.end(); ++it) {
        int required = 1;
        if constexpr (SizedSchedulable<TaskType>) {
            required = it->task.n_qbits();
        }
        if (required <= availableQubits) {
            TaskType task = it->task;
            taskQueue.erase(it);
            return task;
        }
    }
    // Every queued task needs more capacity than is currently available -
    // none is ready to run yet.
    return std::nullopt;
}

template <Schedulable TaskType> std::optional<TaskType> Scheduler<TaskType>::nextMixNMultiLocked() {
    if (taskQueue.empty()) {
        return std::nullopt;
    }
    // A score mixes priority with how long a task has waited, so a
    // low-priority task doesn't starve behind a steady stream of
    // higher-priority arrivals - the same intent as the `Age` field
    // (prevents starvation in the queue).
    auto it = std::max_element(taskQueue.begin(), taskQueue.end(), [&](const Entry &a, const Entry &b) {
        const double scoreA = a.task.priority() + agingWeight * static_cast<double>(nextSequence - a.sequence);
        const double scoreB = b.task.priority() + agingWeight * static_cast<double>(nextSequence - b.sequence);
        if (scoreA != scoreB) {
            return scoreA < scoreB;
        }
        return a.sequence > b.sequence; // tie-break: earlier arrival wins
    });
    TaskType task = it->task;
    taskQueue.erase(it);
    return task;
}

template <Schedulable TaskType> std::optional<TaskType> Scheduler<TaskType>::getNextReadyTask() {
    std::lock_guard<std::mutex> lock(mutex);
    switch (currentPolicy) {
    case SchedulingPolicy::FirstInFirstOut:
    case SchedulingPolicy::PriorityBased:
        return nextFIFOPriorLocked();
    case SchedulingPolicy::RoundRobin:
        return nextRoundRobinLocked();
    case SchedulingPolicy::Backfilling:
        return nextBackfillingLocked();
    case SchedulingPolicy::MixNMulti:
        return nextMixNMultiLocked();
    }
    return std::nullopt;
}

} // namespace mqss

#endif // SCHEDULER_TPP
