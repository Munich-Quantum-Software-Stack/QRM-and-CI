/*-------------------------------------------------------------------------
 This code and any associated documentation is provided "as is"

 IN NO EVENT SHALL LEIBNIZ-RECHENZENTRUM (LRZ) BE LIABLE TO ANY PARTY FOR
 DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 OF THE USE OF THIS CODE AND ITS DOCUMENTATION, EVEN IF LEIBNIZ-RECHENZENTRUM
 (LRZ) HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 THE AFOREMENTIONED EXCLUSIONS OF LIABILITY DO NOT APPLY IN CASE OF INTENT
 BY LEIBNIZ-RECHENZENTRUM (LRZ).

 LEIBNIZ-RECHENZENTRUM (LRZ), SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE.

 THE CODE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, LEIBNIZ-RECHENZENTRUM (LRZ)
 HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 MODIFICATIONS.
 -------------------------------------------------------------------------

@author Martin Letras
  @date   November 2024
  @version 1.0
  @ brief
  This header defines the RabbitMQ Client class, used to communicate CudaQ
  via RabbitMQ to the Munich Quantum Software Stack (MQSS)

 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once
#include "nlohmann/json.hpp"

#include <cstdlib>
#include <filesystem>
#include <map>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#include <string>
#include <uuid/uuid.h> // For generating unique correlation IDs

namespace mqss {
/// @brief The RabbitMQ client exposes an interface
/// to communicate to MQSS
class RabbitMQClient {
public:
  /// @brief Constructor
  RabbitMQClient(const std::string &hostname, int port,
                 const std::string &queue, const std::string &user,
                 const std::string pass);
  /// @brief Destructor
  ~RabbitMQClient();
  std::string sendMessageWithReply(const std::string &request_queue,
                                   const std::string &message,
                                   bool isJson = false);

protected:
  std::string getMessageFromReplyQueue(const std::string &correlation_id,
                                       const std::string &reply_queue);
  std::string generateUUID();

private:
  std::string hostname;
  int port;
  std::string queue;
  amqp_connection_state_t conn;
  amqp_socket_t *socket;
};
} // namespace mqss
