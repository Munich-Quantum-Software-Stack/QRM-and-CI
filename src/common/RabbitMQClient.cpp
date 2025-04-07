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
  RabbitMQ Client class, used to communicate CudaQ
  via RabbitMQ to the Munich Quantum Software Stack (MQSS)

  The following method is exposed to communicate CUDAQ to MQSS
  std::string RabbitMQClient::sendMessageWithReply(const std::string&
request_queue, const std::string& message, bool isJSON)

  1) request_queue might be {RABBITMQ_CUDAQ_LOGIN_QUEUE,
RABBITMQ_CUDAQ_JOB_QUEUE, RABBITMQ_CUDAQ_JOBSTRING_QUEUE } 2) message is the
message itself to be sent 3) isJSON, specifies if the sent message is a json
string

  The function returns as std::string, the response obtained from the MQSS
  CUDAQ has to interpret the result and cast it to json if is required

 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/
#include "mqss/common/RabbitMQClient.hpp"

#include <iostream>

namespace mqss {

RabbitMQClient::RabbitMQClient(const std::string &hostname, int port,
                               const std::string &queue,
                               const std::string &user, const std::string pass)
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
  if (amqp_get_rpc_reply(conn).reply_type != AMQP_RESPONSE_NORMAL)
    throw std::runtime_error("MQSS: Failed to open channel");
}

RabbitMQClient::~RabbitMQClient() {
  amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
  amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection(conn);
  std::cout << "MQSS: RabbitMQ Server shutting down." << std::endl;
}

std::string RabbitMQClient::generateUUID() {
  uuid_t uuid;
  uuid_generate(uuid);
  char str[37];
  uuid_unparse(uuid, str);
  return std::string(str);
}

std::string
RabbitMQClient::getMessageFromReplyQueue(const std::string &correlation_id,
                                         const std::string &reply_queue) {
  // Listen for response
  std::cout << "Correlation id:" << correlation_id << std::endl;
  std::cout << "replyQueue: " << reply_queue << std::endl;
  amqp_basic_consume(conn, 1, amqp_cstring_bytes(reply_queue.c_str()),
                     amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
  while (true) {
    amqp_rpc_reply_t res;
    amqp_envelope_t envelope;
    amqp_maybe_release_buffers(conn);
    res = amqp_consume_message(conn, &envelope, NULL, 0);

    if (res.reply_type != AMQP_RESPONSE_NORMAL)
      continue;
    if (!(envelope.message.properties._flags & AMQP_BASIC_CORRELATION_ID_FLAG))
      continue;
    std::string received_id(
        (char *)envelope.message.properties.correlation_id.bytes,
        envelope.message.properties.correlation_id.len);
    std::cout << "Correlation id a:" << correlation_id << std::endl;
    std::cout << "received id:" << correlation_id << std::endl;
    if (received_id != correlation_id)
      continue;

    // Retrieve and process answer
    std::string message = std::string((char *)envelope.message.body.bytes,
                                      envelope.message.body.len);
    amqp_destroy_envelope(&envelope);
    amqp_queue_delete(conn, 1, amqp_cstring_bytes(reply_queue.c_str()), 0, 0);
    return message;
  }
  return "";
}

// send message via rabbit-mq, if isJson is true, the string message is treated
// as json string
std::string
RabbitMQClient::sendMessageWithReply(const std::string &request_queue,
                                     const std::string &message, bool isJson) {
  std::string correlation_id = generateUUID();

  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG |
                 AMQP_BASIC_CORRELATION_ID_FLAG | AMQP_BASIC_REPLY_TO_FLAG;
  props.content_type =
      amqp_cstring_bytes("text/plain"); // sending message as string
  if (isJson)
    props.content_type =
        amqp_cstring_bytes("application/json"); // specify to send json

  props.delivery_mode = 2; // Persistent delivery mode
  props.correlation_id = amqp_cstring_bytes(correlation_id.c_str());

  // Listen for response
  amqp_queue_declare_ok_t *replyQueue = amqp_queue_declare(
      conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
  std::string reply_queue =
      std::string((char *)replyQueue->queue.bytes, replyQueue->queue.len);

  props.reply_to = amqp_cstring_bytes(reply_queue.c_str());

  std::cout << "before publish" << std::endl;
  // send the message
  amqp_basic_publish(conn, 1, amqp_empty_bytes,
                     amqp_cstring_bytes(request_queue.c_str()), 0, 0, &props,
                     amqp_cstring_bytes(message.c_str()));
  std::cout << "after publish" << std::endl;
  // getting the message from reply queue
  std::cout << "Correlation id " << correlation_id << std::endl;
  return getMessageFromReplyQueue(correlation_id, reply_queue);
}
} // namespace mqss
