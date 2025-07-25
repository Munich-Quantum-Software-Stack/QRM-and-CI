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
        Header containing the definitions useful for rabbitmq.
        Also define the names of the queues connecting each of the modules
        of the QRM.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

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
