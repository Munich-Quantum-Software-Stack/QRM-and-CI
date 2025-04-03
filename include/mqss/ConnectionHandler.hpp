/**
 * @file connection_handling.hpp
 * @brief TODO
 * @todo Comment this source code
 */

#pragma once

#include "rabbitmq-c/amqp.h"
#include "rabbitmq-c/tcp_socket.h"

#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <string>

// Define the RabbitMQ server connection information
#define AMQP_SERVER "127.0.0.1" //"localhost"
#define AMQP_PORT 5672
#define AMQP_USER "guest"
#define AMQP_PASSWORD "guest"
#define AMQP_VHOST "/"
#define RESPONSESIZE 150

#define QUEUE_OFFLOADER_LISTENER "OffloaderListener-to-QRM"
#define QUEUE_QRM_AGNOSTIC_PASS_RUNNER "QRM-To-AgnosticPassRunner"
#define QUEUE_AGNOSTIC_PASS_RUNNER_GENERATOR "AgnosticPassRunner-To-Scheduler"
#define QUEUE_SCHEDULER_TRANSPILER "Scheduler-To-Transpiler"
#define QUEUE_TRANSPILER_SUBMITTER "Transpiler-To-Submitter"
#define QUEUE_SUBMITTER_QRM "Submitter-To-QRM"
