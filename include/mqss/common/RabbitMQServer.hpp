#pragma once

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace mqss {
class RabbitMQServer {
public:
  RabbitMQServer(const std::string &hostname, int port,
                 const std::string &queue, const std::string &user,
                 const std::string &pass);
  ~RabbitMQServer();
  // this method respond to the same queue
  void publishMessage(const std::string &message);
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
