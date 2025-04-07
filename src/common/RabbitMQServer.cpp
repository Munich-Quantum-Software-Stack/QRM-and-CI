#include "mqss/common/RabbitMQServer.hpp"

namespace mqss {
RabbitMQServer::RabbitMQServer(const std::string &hostname, int port,
                               const std::string &queue,
                               const std::string &user, const std::string &pass)
    : hostname(hostname), port(port), queue(queue) {
  conn = amqp_new_connection();
  socket = amqp_tcp_socket_new(conn);
  if (!socket)
    throw std::runtime_error("MQSS: Failed to create TCP socket");

  if (amqp_socket_open(socket, hostname.c_str(), port))
    throw std::runtime_error("MQSS: Failed to open TCP socket");

  amqp_rpc_reply_t login_reply =
      amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user.c_str(),
                 pass.c_str());
  if (login_reply.reply_type != AMQP_RESPONSE_NORMAL)
    throw std::runtime_error("MQSS: Failed to log in to RabbitMQ");

  amqp_channel_open(conn, 1);
  if (amqp_get_rpc_reply(this->conn).reply_type != AMQP_RESPONSE_NORMAL)
    throw std::runtime_error("MQSS: Failed to open channel");
  // Declare the queue to ensure it exists
  amqp_queue_declare_ok_t *queue_declare = amqp_queue_declare(
      conn, 1, amqp_cstring_bytes(queue.c_str()), 0, 1, 0, 0, amqp_empty_table);
  amqp_get_rpc_reply(conn);
  if (!queue_declare) {
    throw std::runtime_error("MQSS: Failed to declare queue");
  }
}

void RabbitMQServer::startToConsume() {
  // Start consuming messages from the queue
  amqp_basic_consume(conn, 1, amqp_cstring_bytes(queue.c_str()),
                     amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
}

RabbitMQServer::~RabbitMQServer() {
  amqp_channel_close(this->conn, 1, AMQP_REPLY_SUCCESS);
  amqp_connection_close(this->conn, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection(this->conn);
  // std::cout << "MQSS: RabbitMQ Server shutting down." << std::endl;
}

void RabbitMQServer::publishMessage(const std::string &message, bool isJson) {
  amqp_bytes_t queueBytes = amqp_cstring_bytes(this->queue.c_str());
  amqp_bytes_t msg_bytes = amqp_cstring_bytes(message.c_str());
  amqp_basic_properties_t props;
  props.content_type = amqp_cstring_bytes("text/plain");
  if (isJson)
    props.content_type = amqp_cstring_bytes("application/json");
  // sending to queue
  amqp_basic_publish(conn, 1, amqp_empty_bytes, queueBytes, 0, 0, &props,
                     msg_bytes);
}

void RabbitMQServer::publishMessage(const std::string &reply_to,
                                    const std::string &message,
                                    const std::string &correlation_id,
                                    bool isJson) {
  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_CORRELATION_ID_FLAG;
  props.content_type = amqp_cstring_bytes("text/plain");
  if (isJson)
    props.content_type = amqp_cstring_bytes("application/json");
  // std::cout << "Answer correlation id: " << correlation_id << std::endl;
  props.correlation_id = amqp_cstring_bytes(correlation_id.c_str());
  // sending the response
  amqp_basic_publish(conn, 1, amqp_empty_bytes,
                     amqp_cstring_bytes(reply_to.c_str()), 0, 0, &props,
                     amqp_cstring_bytes(message.c_str()));
}

void RabbitMQServer::consumeMessage(std::string &message) {
  message = ""; // if not success, return empty string
  amqp_rpc_reply_t res;
  amqp_envelope_t envelope;

  amqp_maybe_release_buffers(conn);
  res = amqp_consume_message(conn, &envelope, NULL, 0);
  if (res.reply_type == AMQP_RESPONSE_NORMAL) {
    std::string message =
        std::string(static_cast<char *>(envelope.message.body.bytes),
                    envelope.message.body.len);
    amqp_destroy_envelope(&envelope);
  }
}

void RabbitMQServer::consumeMessage(amqp_envelope_t &envelope,
                                    std::string &message, std::string &reply_to,
                                    std::string &correlation_id) {
  message = ""; // if not success, return empty string
  amqp_rpc_reply_t res;
  // Attempt to get the next message from the queue
  res = amqp_consume_message(conn, &envelope, NULL, 0);
  if (res.reply_type == AMQP_RESPONSE_NORMAL) {
    reply_to = std::string((char *)envelope.message.properties.reply_to.bytes,
                           envelope.message.properties.reply_to.len);
    correlation_id =
        std::string((char *)envelope.message.properties.correlation_id.bytes,
                    envelope.message.properties.correlation_id.len);
    // Retrieve and process message
    message = std::string((char *)envelope.message.body.bytes,
                          envelope.message.body.len);
  }
}
} // namespace mqss
