/* This code and any associated documentation is provided "as is"

Copyright 2024 Munich Quantum Software Stack Project

Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
"License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

TODO

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-------------------------------------------------------------------------
  author Martin Letras
  date   April 2025
  version 1.0
  brief
        Base class defining a RabbitMQ server.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#pragma once

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <stdexcept>
#include <string>

namespace mqss {
class RabbitMQServer {
public:
  RabbitMQServer(const std::string &hostname, int port,
                 const std::string &queue, const std::string &user,
                 const std::string &pass);
  ~RabbitMQServer();
  void startToConsume();
  // this method respond to the same queue
  void publishMessage(const std::string &message, bool isJson = false);
  // I guess this allows to publish to specific connections
  void publishMessage(const std::string &reply_to, const std::string &message,
                      const std::string &correlation_id, bool isJson = false);
  void consumeMessage(std::string &message);
  void consumeMessage(amqp_envelope_t &envelope, std::string &message,
                      std::string &reply_to, std::string &correlation_id);

private:
  std::string hostname;
  int port;
  std::string queue;
  amqp_connection_state_t conn;
  amqp_socket_t *socket;
};
} // namespace mqss
