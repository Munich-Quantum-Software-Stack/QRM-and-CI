/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "Config.hpp"
#include "Submitter.hpp"
#include "mqss/Messenger.hpp"
#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mqss::qrmci {

/// @file qrmci.hpp
/// @brief QRMCI core types and messaging helpers.

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
  CommunicationHandler(const mqss::qrmci::RabbitMqConfig &config)
    requires(std::same_as<Communicator, mqss::RabbitMqSimple>);

  /// @brief Send a quantum task to the specified queue.
  /// @param task The quantum task to be sent.
  /// @param queue_name The name of the queue that receives the task.
  void send_quantum_task(const mqss::QuantumTask &task,
                         const std::string &queue_name);

  /// @brief Send a quantum result to the specified queue.
  /// @param result The quantum result to be sent.
  /// @param queue_name The name of the queue that receives the result.
  void send_quantum_result(const mqss::QuantumResult &result,
                           const std::string &queue_name);

  /// @brief Retrieve the next quantum task from the specified queue.
  /// @param queue_name The name of the queue to consume from.
  /// @param timeout The maximum time to wait for a message before returning. 0
  /// means wait indefinitely.
  /// @param termination_flag If true, the function will return immediately if
  ///        the queue is empty, otherwise it will block until a message is
  ///        received or the timeout is reached.
  /// @return The next quantum task.
  /// @note This function will block until a message is received or the timeout
  ///       is reached. If the termination_flag is set to true, the function
  ///       will return immediately if the queue is empty.
  std::optional<mqss::QuantumTask> get_next_quantum_task(
      const std::string &queue_name,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(500),
      const std::atomic<bool> &termination_flag = std::atomic<bool>(false));

  /// @brief Retrieve the next quantum result from the specified queue.
  /// @param queue_name The name of the queue to consume from.
  /// @param timeout The maximum time to wait for a message before returning. 0
  /// means wait indefinitely.
  /// @param termination_flag If true, the function will return immediately if
  ///        the queue is empty, otherwise it will block until a message is
  ///        received or the timeout is reached.
  /// @return The next quantum result.
  /// @note This function will block until a message is received or the timeout
  ///       is reached. If the termination_flag is set to true, the function
  ///       will return immediately if the queue is empty.
  std::optional<mqss::QuantumResult> get_next_quantum_result(
      const std::string &queue_name,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(500),
      const std::atomic<bool> &termination_flag = std::atomic<bool>(false));

private:
  mqss::Messenger<Communicator, SerializationFormat> messenger_;
};

/// @brief Supported backend categories for scheduling decisions.
enum class BackendType {
  Superconducting,
  TrappedIon,
  NeutralAtom,
  Simulator,
  // Add more backend types as needed
};

/// @brief Represents a quantum backend and the metadata used for selection.
struct Backend {
  std::string name;       ///< Backend display name.
  std::string queue_name; ///< Queue name associated with the backend.
  BackendType type;       ///< Backend category.
  int num_qubits;         ///< Number of qubits available on the backend.
  float current_load;     ///< Relative backend load.
};

/// @brief Selects the backend that should execute a quantum task.
/// @param task Update the quantum task with the selected backend.
/// @param available_backends A list of available backends to choose from.
void select_backend(mqss::QuantumTask &task,
                    const std::vector<Backend> &available_backends);

/// @brief Compile the quantum task using the MQSS compiler.
/// @param task The quantum task to compile.
void compile_quantum_task(mqss::QuantumTask &task);

/// @brief Submit the quantum task to the submitter for execution.
/// @param task The quantum task to submit.
/// @param submitter The submitter to use for task submission.
/// @return The result of the quantum task execution.
mqss::QuantumResult submit_quantum_task(const mqss::QuantumTask &task,
                                        mqss::Submitter &submitter);

} // namespace mqss::qrmci
