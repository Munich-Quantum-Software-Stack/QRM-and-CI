/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci.hpp"

#include <iostream>
#include <stdexcept>

namespace mqss::qrmci {

template <class Communicator, class SerializationFormat>
CommunicationHandler<Communicator, SerializationFormat>::CommunicationHandler(
    const mqss::qrmci::RabbitMqConfig &config)
  requires(std::same_as<Communicator, mqss::RabbitMqSimple>)
    : messenger_(mqss::TransportOptions<Communicator>{
          .host = std::string(config.host),
          .port = config.port,
          .username = std::string(config.user),
          .password = std::string(config.password),
      }) {}

template <class Communicator, class SerializationFormat>
void CommunicationHandler<Communicator, SerializationFormat>::send_quantum_task(
    const mqss::QuantumTask &task, const std::string &queue_name) {
  auto send_st = messenger_.template send<mqss::QuantumTask>(
      {std::string(queue_name)}, task);

  if (!send_st.ok()) {
    throw std::runtime_error("Failed to send quantum task " +
                             std::to_string(task.task_id()) + ": " +
                             send_st.reason());
  }
}

template <class Communicator, class SerializationFormat>
void CommunicationHandler<Communicator, SerializationFormat>::
    send_quantum_result(const mqss::QuantumResult &result,
                        const std::string &queue_name) {
  auto send_st = messenger_.template send<mqss::QuantumResult>(
      {std::string(queue_name)}, result);

  if (!send_st.ok()) {
    throw std::runtime_error("Failed to send quantum result for task " +
                             std::to_string(result.task_id()) + ": " +
                             send_st.reason());
  }
}

template <class Communicator, class SerializationFormat>
std::optional<mqss::QuantumTask>
CommunicationHandler<Communicator, SerializationFormat>::get_next_quantum_task(
    const std::string &queue_name, std::chrono::milliseconds timeout,
    const std::atomic<bool> &termination_flag) {

  auto _timeout = timeout > std::chrono::milliseconds(0)
                      ? timeout
                      : std::chrono::milliseconds(500);
  do {
    auto res = messenger_.template receive<mqss::QuantumTask>(
        {queue_name}, mqss::ReceiveArgs{
                          .timeout = _timeout,
                          .ack_mode = mqss::AckMode::Auto,
                      });
    if (res.has_value()) {
      return *res;
    }
    if (!res.has_value() && res.error().code() != mqss::StatusCode::Timeout) {

      throw std::runtime_error("Receive error: " + res.error().reason());
    }
  } while (!termination_flag && timeout == std::chrono::milliseconds(0));
  return std::nullopt;
}

template <class Communicator, class SerializationFormat>
std::optional<mqss::QuantumResult>
CommunicationHandler<Communicator, SerializationFormat>::
    get_next_quantum_result(const std::string &queue_name,
                            std::chrono::milliseconds timeout,
                            const std::atomic<bool> &termination_flag) {

  auto _timeout = timeout > std::chrono::milliseconds(0)
                      ? timeout
                      : std::chrono::milliseconds(500);
  do {
    auto res = messenger_.template receive<mqss::QuantumResult>(
        {queue_name}, mqss::ReceiveArgs{
                          .timeout = _timeout,
                          .ack_mode = mqss::AckMode::Auto,
                      });

    if (res.has_value()) {
      return *res;
    }
    if (!res.has_value() && res.error().code() != mqss::StatusCode::Timeout) {
      throw std::runtime_error("Receive error: " + res.error().reason());
    }
  } while (!termination_flag && timeout == std::chrono::milliseconds(0));
  return std::nullopt;
}

template class CommunicationHandler<mqss::RabbitMqSimple, mqss::ProtoJson>;

} // namespace mqss::qrmci
