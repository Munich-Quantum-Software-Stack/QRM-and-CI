/**
 * @file connection_handling.hpp
 * @brief TODO
 * @todo Comment this source code
 */

#pragma once

// Define the RabbitMQ server connection information
#define AMQP_SERVER "127.0.0.1" //"localhost"
#define AMQP_PORT 5672
#define AMQP_USER "guest"
#define AMQP_PASSWORD "guest"
#define AMQP_VHOST "/"
#define RESPONSESIZE 150

#define QUEUE_HPC_OFFLOADER "MQSS-HPC-Offloader"
#define QUEUE_MQP_OFFLOADER "MQSS-MQP-Offloader"
#define QUEUE_QRM_AGNOSTIC_PASS_RUNNER "MQSS-To-AgnosticPassRunner"
#define QUEUE_AGNOSTIC_PASS_RUNNER_SCHEDULER "MQSS-To-Scheduler"
#define QUEUE_SCHEDULER_TRANSPILER "MQSS-To-Transpiler"
#define QUEUE_TRANSPILER_SUBMITTER "MQSS-To-Submitter"
#define QUEUE_SUBMITTER_OFFLOADER "MQSS-To-Offloader"
#define QUEUE_SUBMITTER_BACKEND "MQSS-To-Backend" // for Testing
#define QUEUE_BACKEND_SUBMITTER "MQSS-To-Backend" // for Testing
