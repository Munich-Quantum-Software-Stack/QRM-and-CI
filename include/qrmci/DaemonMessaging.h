/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file DaemonMessaging.h
/// @brief The queue I/O every QRM&CI daemon work loop shares: receiving and
///        draining a queue, and the best-effort sends a work loop makes,
///        including telling a task's submitter how it ended.

#pragma once

#include "qrmci/CommunicationHandler.h"
#include "qrmci/Error.h"
#include "qrmci/Runners.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>

namespace mqss::qrmci {

/// @brief Receive one message from a queue and hand it to @p handle.
///
/// Neither an empty queue nor a broker failure is fatal to a work loop: both
/// simply mean there is nothing to do this turn, the second after a warning.
/// @tparam Message The message type to receive.
/// @tparam Handler Callable as handle(Message &&).
/// @param comms The handler to receive through.
/// @param queueName The queue to consume from.
/// @param timeout How long to wait; 0ms polls once and returns immediately.
/// @param terminationFlag Consulted before the receive attempt (and between
///        polls of an indefinite wait), so a shutdown interrupts the wait
///        rather than outliving it. The caller supplies this explicitly --
///        a daemon passes its own process-wide flag; a test or short-lived
///        tool that never shuts down can pass one that is never set.
/// @param handle Invoked with the received message, if one arrived.
/// @return True if a message was received and handed to @p handle.
template <class Message, class Handler>
bool receiveNext(CommunicationHandler &comms, const std::string &queueName,
                 std::chrono::milliseconds timeout,
                 const std::atomic<bool> &terminationFlag, Handler &&handle) {
  auto received = comms.receive<Message>(queueName, timeout, terminationFlag);
  if (!received) {
    spdlog::warn("Could not read from queue '{}': {}", queueName,
                 received.error().detail);
    return false;
  }
  if (!received->has_value()) {
    return false;
  }
  std::invoke(std::forward<Handler>(handle), std::move(**received));
  return true;
}

/// @brief Receive from a queue until it is empty or the broker fails,
///        handing every message to @p handle.
/// @tparam Message The message type to receive.
/// @tparam Handler Callable as handle(Message &&), invoked once per message.
/// @param comms The handler to receive through.
/// @param queueName The queue to drain.
/// @param terminationFlag Consulted before each poll; see receiveNext()'s
///        own documentation.
/// @param handle Invoked with each received message.
/// @return True if at least one message was handled.
template <class Message, class Handler>
bool drainQueue(CommunicationHandler &comms, const std::string &queueName,
                const std::atomic<bool> &terminationFlag, Handler &&handle) {
  bool handledAny = false;
  // A single non-blocking poll per turn: the queue is drained by looping,
  // not by waiting, so an empty queue costs one round trip rather than a
  // timeout.
  while (receiveNext<Message>(comms, queueName, std::chrono::milliseconds(0),
                              terminationFlag, handle)) {
    handledAny = true;
  }
  return handledAny;
}

/// @brief Send a message, logging rather than propagating a failure.
///
/// Every send a running daemon makes is best-effort: a broker that rejects a
/// message must not cost the daemon its in-flight work, so a failure is
/// logged and the loop carries on.
/// @tparam Message The message type to send.
/// @param comms The handler to send through.
/// @param message The message to send.
/// @param queueName The queue to send it to.
template <class Message>
void send(CommunicationHandler &comms, const Message &message,
          const std::string &queueName) {
  if (auto sent = comms.send(message, queueName); !sent) {
    spdlog::warn("{}", sent.error().detail);
  }
}

/// @brief Tell a task's submitter that the task failed, and log why.
/// @param comms The handler to send through.
/// @param task The task that failed.
/// @param failure The failure to report, carried into the cancellation
///        message the submitter receives.
/// @param stage What the daemon was doing, e.g. `could not be scheduled`.
inline void sendFailure(CommunicationHandler &comms,
                        const mqss::QuantumTask &task, const Error &failure,
                        std::string_view stage) {
  send(comms, cancelQuantumTask(task, failure.detail),
       task.result_destination());
  spdlog::warn("Task {} {} [{}]: {}", task.task_id(), stage,
               toString(failure.kind), failure.detail);
}

/// @brief Send a task's result to its own result destination, and log it.
/// @param comms The handler to send through.
/// @param task The task the result belongs to.
/// @param result The result to send.
inline void sendResult(CommunicationHandler &comms,
                       const mqss::QuantumTask &task,
                       const mqss::QuantumResult &result) {
  send(comms, result, task.result_destination());
  spdlog::info("Job Executed successfully. Results for task {} sent to '{}'.",
               task.task_id(), task.result_destination());
}

} // namespace mqss::qrmci
