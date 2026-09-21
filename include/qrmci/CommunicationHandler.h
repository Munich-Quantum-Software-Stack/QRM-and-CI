/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file CommunicationHandler.h
/// @brief The RabbitMQ send/receive helpers that carry tasks, results and
///        backend status between QRM&CI stages.

#pragma once

#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"
#include "qrmci/Config.h"
#include "qrmci/Error.h"

#include <atomic>
#include <chrono>
#include <expected>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

namespace mqss::qrmci {

/// @brief How long a single receive attempt waits when the caller does not
///        say, and the interval an indefinite receive polls at.
inline constexpr std::chrono::milliseconds DefaultReceiveTimeout{500};

/// @brief Timeout value meaning "poll until a message arrives or the
///        termination flag is set" rather than returning after a bounded
///        wait.
///
/// A negative sentinel rather than 0ms, because 0ms already means "poll once
/// and return immediately if nothing is available" -- the same convention the
/// underlying transport uses for ReceiveArgs::timeout.
inline constexpr std::chrono::milliseconds WaitForever{-1};

/// @brief Translate a RabbitMqConnectionConfig into the transport's own
///        options. Exposed (rather than kept file-local) so a test can
///        assert that every configured field actually reaches the
///        transport, instead of only asserting the struct it was given.
[[nodiscard]] mqss::TransportOptions<mqss::RabbitMqSimple>
makeTransportOptions(const RabbitMqConnectionConfig &config);

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

/// @brief No generic fallback: a message type with no more specific overload
///        above must fail to compile here rather than silently receive the
///        generic label `message` in every log line and error it appears in.
/// @tparam Message The message type being described.
template <class Message>
std::string messageLabel(const Message & /*unused*/) = delete;

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
  explicit CommunicationHandler(const RabbitMqConnectionConfig &config);

  /// @brief Explicitly non-copyable and non-movable: a moved-from handler
  ///        would leave its messenger's transport connection in an unusable
  ///        state with no way to tell from the type alone, so that state is
  ///        refused at compile time rather than left implicit. Construct one
  ///        in place instead.
  CommunicationHandler(const CommunicationHandler &) = delete;
  CommunicationHandler &operator=(const CommunicationHandler &) = delete;
  CommunicationHandler(CommunicationHandler &&) = delete;
  CommunicationHandler &operator=(CommunicationHandler &&) = delete;

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
    auto sendStatus = messenger.send<Message>({queueName}, message);

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
  /// @param timeout The maximum time to wait for a message. 0ms polls once
  ///        and returns immediately if nothing is available; pass
  ///        WaitForever to poll until a message arrives or @p
  ///        terminationFlag is set.
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
    const bool waitIndefinitely = (timeout == WaitForever);
    const auto pollInterval =
        waitIndefinitely ? DefaultReceiveTimeout : timeout;

    // The flag is the loop condition rather than a second conjunct alongside
    // the timeout, so it is honoured at every timeout instead of only at 0.
    while (!terminationFlag.load(std::memory_order_acquire)) {
      // Manual ack keeps the broker-side no_ack flag false, so RabbitMQ's
      // per-consumer prefetch actually throttles delivery instead of pushing
      // every queued message onto this call's short-lived connection at once.
      // The message is acked right after a successful decode.
      auto received = messenger.receiveExtended<Message>(
          {queueName}, mqss::ReceiveArgs{
                           .timeout = pollInterval,
                           .ack_mode = mqss::AckMode::Manual,
                       });
      if (received.has_value()) {
        auto &[message, delivery] = *received;
        if (auto ackStatus = delivery.ack(); !ackStatus.ok()) {
          spdlog::warn("Failed to ack {}: {}", messageLabel(message),
                       ackStatus.reason());
        }
        return std::optional<Message>(std::move(message));
      }
      if (received.error().code() != mqss::StatusCode::Timeout) {
        return std::unexpected(
            Error{Error::Kind::MessagingFailed,
                  "Receive error: " + received.error().reason()});
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
  /// A separate overload rather than a defaulted parameter, so that a caller
  /// with no flag to offer has to say so rather than silently binding one
  /// that can never be set. Tests and short-lived tools are the intended
  /// users; a daemon should pass its own flag.
  /// @tparam Message The message type to receive.
  /// @param queueName The name of the queue to consume from.
  /// @param timeout The maximum time to wait for a message. 0ms polls once
  ///        and returns immediately if nothing is available; WaitForever is
  ///        uninterruptible on this overload since there is no flag to
  ///        consult, so pass one from any process that has to shut down.
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
