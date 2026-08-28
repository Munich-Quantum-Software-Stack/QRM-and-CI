/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/CommunicationHandler.h"

#include "qrmci/Config.h"

#include <iostream>
#include <stdexcept>

namespace mqss::qrmci {

template <class Communicator, class SerializationFormat>
CommunicationHandler<Communicator, SerializationFormat>::CommunicationHandler(
    const mqss::qrmci::RabbitMqConfig &config)
  requires(std::same_as<Communicator, mqss::RabbitMqSimple>)
    : messenger(mqss::TransportOptions<Communicator>{
          .host = std::string(config.host),
          .port = config.port,
          .username = std::string(config.user),
          .password = std::string(config.password),
      }) {}

template <class Communicator, class SerializationFormat>
void CommunicationHandler<Communicator, SerializationFormat>::sendQuantumTask(
    const mqss::QuantumTask &task, const std::string &queueName) {
  auto sendStatus = messenger.template send<mqss::QuantumTask>(
      {std::string(queueName)}, task);

  if (!sendStatus.ok()) {
    throw std::runtime_error("Failed to send quantum task " +
                             std::to_string(task.task_id()) + ": " +
                             sendStatus.reason());
  }
}

template <class Communicator, class SerializationFormat>
void CommunicationHandler<Communicator, SerializationFormat>::sendQuantumResult(
    const mqss::QuantumResult &result, const std::string &queueName) {
  auto sendStatus = messenger.template send<mqss::QuantumResult>(
      {std::string(queueName)}, result);

  if (!sendStatus.ok()) {
    throw std::runtime_error("Failed to send quantum result for task " +
                             std::to_string(result.task_id()) + ": " +
                             sendStatus.reason());
  }
}

template <class Communicator, class SerializationFormat>
std::optional<mqss::QuantumTask>
CommunicationHandler<Communicator, SerializationFormat>::getNextQuantumTask(
    const std::string &queueName, std::chrono::milliseconds timeout,
    const std::atomic<bool> &terminationFlag) {

  auto localTimeout = timeout > std::chrono::milliseconds(0)
                          ? timeout
                          : std::chrono::milliseconds(500);
  do { // NOLINT(cppcoreguidelines-avoid-do-while)
    // Manual ack (rather than Auto) matters here: the transport opens a new
    // channel per receive() call and only ever consumes one message from it
    // before cancelling. With Auto ack, RabbitMQ still pushes every message
    // already queued down that channel unthrottled (auto-ack disables
    // prefetch-based flow control), so anything beyond the one message this
    // call actually reads is acked-and-removed server-side yet never
    // delivered to the app - permanently lost. Manual ack keeps prefetch=1
    // in effect, so RabbitMQ holds back further messages until we ack, and
    // requeues this one if the channel is torn down before we do.
    auto res = messenger.template receiveExtended<mqss::QuantumTask>(
        {queueName}, mqss::ReceiveArgs{
                         .timeout = localTimeout,
                         .ack_mode = mqss::AckMode::Manual,
                     });
    if (res.has_value()) {
      auto &[task, msg] = *res;
      if (auto ackStatus = msg.ack(); !ackStatus.ok()) {
        std::cerr << "Warning: failed to ack task " << task.task_id() << ": "
                  << ackStatus.reason() << "\n";
      }
      return task;
    }
    if (!res.has_value() && res.error().code() != mqss::StatusCode::Timeout) {

      throw std::runtime_error("Receive error: " + res.error().reason());
    }
  } while (!terminationFlag && timeout == std::chrono::milliseconds(0));
  return std::nullopt;
}

template <class Communicator, class SerializationFormat>
std::optional<mqss::QuantumResult>
CommunicationHandler<Communicator, SerializationFormat>::getNextQuantumResult(
    const std::string &queueName, std::chrono::milliseconds timeout,
    const std::atomic<bool> &terminationFlag) {

  auto localTimeout = timeout > std::chrono::milliseconds(0)
                          ? timeout
                          : std::chrono::milliseconds(500);
  do { // NOLINT(cppcoreguidelines-avoid-do-while)
    // See getNextQuantumTask() for why Manual ack is required here.
    auto res = messenger.template receiveExtended<mqss::QuantumResult>(
        {queueName}, mqss::ReceiveArgs{
                         .timeout = localTimeout,
                         .ack_mode = mqss::AckMode::Manual,
                     });

    if (res.has_value()) {
      auto &[result, msg] = *res;
      if (auto ackStatus = msg.ack(); !ackStatus.ok()) {
        std::cerr << "Warning: failed to ack result for task "
                  << result.task_id() << ": " << ackStatus.reason() << "\n";
      }
      return result;
    }
    if (!res.has_value() && res.error().code() != mqss::StatusCode::Timeout) {
      throw std::runtime_error("Receive error: " + res.error().reason());
    }
  } while (!terminationFlag && timeout == std::chrono::milliseconds(0));
  return std::nullopt;
}

template class CommunicationHandler<mqss::RabbitMqSimple, mqss::ProtoJson>;

} // namespace mqss::qrmci
