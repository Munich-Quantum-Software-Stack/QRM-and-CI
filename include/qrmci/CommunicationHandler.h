/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "Config.h"
#include "Error.h"
#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"

#include <atomic>
#include <chrono>
#include <expected>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

namespace mqss::qrmci {

/// @file CommunicationHandler.h
/// @brief Messaging helpers for QRMCI.

/// @brief How long a single receive attempt waits when the caller does not
///        say, and the interval an indefinite receive polls at.
inline constexpr std::chrono::milliseconds DefaultReceiveTimeout{500};

/// @brief Describe a quantum task for an error or log line.
/// @param task The task to describe.
/// @return A human-readable label, e.g. `quantum task 7`.
inline std::string messageLabel(const mqss::QuantumTask &task) {
  return "quantum task " + std::to_string(task.task_id());
}

/// @brief Describe a quantum result for an error or log line.
/// @param result The result to describe.
/// @return A human-readable label, e.g. `quantum result for task 7`.
inline std::string messageLabel(const mqss::QuantumResult &result) {
  return "quantum result for task " + std::to_string(result.task_id());
}

/// @brief Describe a backend status snapshot for an error or log line.
/// @param backend The backend status to describe.
/// @return A human-readable label, e.g. `backend status for backend alpha`.
inline std::string messageLabel(const mqss::Backend &backend) {
  return "backend status for backend " + backend.name();
}

/// @brief Fallback label for a message type with no more specific overload.
/// @tparam Message The message type being described.
/// @return The generic label `message`.
template <class Message> std::string messageLabel(const Message & /*unused*/) {
  return "message";
}

/// @brief CommunicationHandler abstracts the messaging operations that move
///        pipeline messages between QRM&CI stages.
///
/// One send and one receive cover every message type: the type is a template
/// parameter of the operation, not part of its name, so a new pipeline
/// message costs a `messageLabel` overload rather than a new pair of methods.
/// Neither operation throws; both report failure through `std::expected`, as
/// the orchestration layer in Runners.h does, so a daemon loop can log a
/// broker failure and keep its in-flight work instead of unwinding.
class CommunicationHandler {
public:
  /// @brief Construct a CommunicationHandler with the given configuration.
  /// @param config The RabbitMQ configuration to use for the connection.
  explicit CommunicationHandler(
      const mqss::qrmci::RabbitMqConnectionConfig &config);

  /// @brief Send a message to the specified queue.
  /// @tparam Message The message type to send (e.g. mqss::QuantumTask,
  ///         mqss::QuantumResult, mqss::Backend).
  /// @param message The message to be sent.
  /// @param queueName The name of the queue that receives the message.
  /// @return Nothing on success, or a MessagingFailed error describing the
  ///         failure. The message is not queued for retry, so a caller that
  ///         must not lose it has to resend it itself.
  template <class Message>
  [[nodiscard]] std::expected<void, Error> send(const Message &message,
                                                const std::string &queueName) {
    auto sendStatus = messenger.template send<Message>({queueName}, message);

    if (!sendStatus.ok()) {
      return std::unexpected(Error{Error::Kind::MessagingFailed,
                                   "Failed to send " + messageLabel(message) +
                                       ": " + sendStatus.reason()});
    }
    return {};
  }

  /// @brief Retrieve the next message of the given type from the specified
  ///        queue.
  /// @tparam Message The message type to receive (e.g. mqss::QuantumTask,
  ///         mqss::QuantumResult, mqss::Backend).
  /// @param queueName The name of the queue to consume from.
  /// @param timeout The maximum time to wait for a message. 0 means poll
  ///        until a message arrives or @p terminationFlag is set.
  /// @param terminationFlag Consulted before every receive attempt: once set,
  ///        this returns an empty optional without touching the transport,
  ///        whatever the timeout.
  /// @return The next message, an empty optional if the timeout elapsed or
  ///         termination was requested, or a MessagingFailed error describing
  ///         the failure.
  /// @note The message is received under manual ack and explicitly
  ///       acknowledged once successfully decoded.
  template <class Message>
  [[nodiscard]] std::expected<std::optional<Message>, Error>
  receive(const std::string &queueName, std::chrono::milliseconds timeout,
          const std::atomic<bool> &terminationFlag) {
    const bool waitIndefinitely = timeout <= std::chrono::milliseconds(0);
    const auto pollInterval =
        waitIndefinitely ? DefaultReceiveTimeout : timeout;

    // The flag is the loop condition rather than a second conjunct alongside
    // the timeout, so it is honoured at every timeout instead of only at 0.
    while (!terminationFlag.load(std::memory_order_acquire)) {
      // Manual ack keeps the broker-side no_ack flag false, so RabbitMQ's
      // per-consumer prefetch actually throttles delivery instead of pushing
      // every queued message onto this call's short-lived connection at once.
      // The message is acked right after a successful decode, matching the
      // previous auto-ack-on-receipt behavior.
      auto res = messenger.template receiveExtended<Message>(
          {queueName}, mqss::ReceiveArgs{
                           .timeout = pollInterval,
                           .ack_mode = mqss::AckMode::Manual,
                       });
      if (res.has_value()) {
        auto &[message, delivery] = *res;
        if (auto ackStatus = delivery.ack(); !ackStatus.ok()) {
          spdlog::warn("Failed to ack {}: {}", messageLabel(message),
                       ackStatus.reason());
        }
        return std::optional<Message>(std::move(message));
      }
      if (res.error().code() != mqss::StatusCode::Timeout) {
        return std::unexpected(Error{Error::Kind::MessagingFailed,
                                     "Receive error: " + res.error().reason()});
      }
      if (!waitIndefinitely) {
        break;
      }
    }
    return std::optional<Message>{};
  }

  /// @brief Retrieve the next message of the given type, with no termination
  ///        flag to interrupt the wait.
  ///
  /// Replaces a default argument that bound a const reference to a temporary
  /// `std::atomic<bool>`. A caller with no flag to offer now omits the
  /// parameter instead of silently receiving one that can never be set.
  /// @tparam Message The message type to receive.
  /// @param queueName The name of the queue to consume from.
  /// @param timeout The maximum time to wait for a message. 0 means poll
  ///        until a message arrives -- uninterruptible without a flag, so
  ///        pass one from any process that has to shut down.
  /// @return The next message, an empty optional if the timeout elapsed, or a
  ///         MessagingFailed error describing the failure.
  template <class Message>
  [[nodiscard]] std::expected<std::optional<Message>, Error>
  receive(const std::string &queueName,
          std::chrono::milliseconds timeout = DefaultReceiveTimeout) {
    static const std::atomic<bool> NeverTerminates{false};
    return receive<Message>(queueName, timeout, NeverTerminates);
  }

private:
  mqss::Messenger<mqss::RabbitMqSimple, mqss::ProtoJson> messenger;
};
} // namespace mqss::qrmci
