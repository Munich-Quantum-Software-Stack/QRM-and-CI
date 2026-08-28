/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/CommunicationHandler.h"

#include "qrmci/Config.h"

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
    auto res = messenger.template receive<mqss::QuantumTask>(
        {queueName}, mqss::ReceiveArgs{
                         .timeout = localTimeout,
                         .ack_mode = mqss::AckMode::Auto,
                     });
    if (res.has_value()) {
      return *res;
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
    auto res = messenger.template receive<mqss::QuantumResult>(
        {queueName}, mqss::ReceiveArgs{
                         .timeout = localTimeout,
                         .ack_mode = mqss::AckMode::Auto,
                     });

    if (res.has_value()) {
      return *res;
    }
    if (!res.has_value() && res.error().code() != mqss::StatusCode::Timeout) {
      throw std::runtime_error("Receive error: " + res.error().reason());
    }
  } while (!terminationFlag && timeout == std::chrono::milliseconds(0));
  return std::nullopt;
}

template class CommunicationHandler<mqss::RabbitMqSimple, mqss::ProtoJson>;

} // namespace mqss::qrmci
