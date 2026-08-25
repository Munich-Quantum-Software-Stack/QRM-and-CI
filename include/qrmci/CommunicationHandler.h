/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "Config.h"
#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <optional>
#include <string>

namespace mqss::qrmci {

/// @file CommunicationHandler.h
/// @brief Messaging helpers for QRMCI.

/// @brief CommunicationHandler is a template class that abstracts the messaging
///        operations for sending and receiving quantum tasks and results.
/// @tparam Communicator The communication protocol to use (e.g.,
/// RabbitMqSimple).
/// @tparam SerializationFormat The serialization format for messages (e.g.,
/// ProtoJson).
template <class Communicator, class SerializationFormat>
class CommunicationHandler {
public:
  /// @brief Construct a CommunicationHandler with the given configuration.
  /// @param config The RabbitMQ configuration to use for the communicator.
  explicit CommunicationHandler(const mqss::qrmci::RabbitMqConfig &config)
    requires(std::same_as<Communicator, mqss::RabbitMqSimple>);

  /// @brief Send a quantum task to the specified queue.
  /// @param task The quantum task to be sent.
  /// @param queueName The name of the queue that receives the task.
  void sendQuantumTask(const mqss::QuantumTask &task,
                       const std::string &queueName);

  /// @brief Send a quantum result to the specified queue.
  /// @param result The quantum result to be sent.
  /// @param queueName The name of the queue that receives the result.
  void sendQuantumResult(const mqss::QuantumResult &result,
                         const std::string &queueName);

  /// @brief Retrieve the next quantum task from the specified queue.
  /// @param queueName The name of the queue to consume from.
  /// @param timeout The maximum time to wait for a message before returning. 0
  /// means wait indefinitely.
  /// @param terminationFlag If true, the function will return immediately if
  ///        the queue is empty, otherwise it will block until a message is
  ///        received or the timeout is reached.
  /// @return The next quantum task.
  /// @note This function will block until a message is received or the timeout
  ///       is reached. If the termination_flag is set to true, the function
  ///       will return immediately if the queue is empty.
  std::optional<mqss::QuantumTask> getNextQuantumTask(
      const std::string &queueName,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(500),
      const std::atomic<bool> &terminationFlag = std::atomic<bool>(false));

  /// @brief Retrieve the next quantum result from the specified queue.
  /// @param queueName The name of the queue to consume from.
  /// @param timeout The maximum time to wait for a message before returning. 0
  /// means wait indefinitely.
  /// @param terminationFlag If true, the function will return immediately if
  ///        the queue is empty, otherwise it will block until a message is
  ///        received or the timeout is reached.
  /// @return The next quantum result.
  /// @note This function will block until a message is received or the timeout
  ///       is reached. If the termination_flag is set to true, the function
  ///       will return immediately if the queue is empty.
  std::optional<mqss::QuantumResult> getNextQuantumResult(
      const std::string &queueName,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(500),
      const std::atomic<bool> &terminationFlag = std::atomic<bool>(false));

private:
  mqss::Messenger<Communicator, SerializationFormat> messenger;
};
} // namespace mqss::qrmci
