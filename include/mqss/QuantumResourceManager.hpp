/**
 * @file QuantumResourceManager.hpp
 * @brief TODO
 */
#pragma once

#include "ConnectionHandler.hpp"
#include "QuantumTask.hpp"

/**
 * @todo Document this
 */

using namespace mqss;

QuantumTask JSONToQuantumTask(const char *QuantumTask_str);
void handleQuantumDaemon(amqp_connection_state_t &conn, char const *QDQueue,
                         const QuantumTask &quantumTask);
void signalHandler(int signum);
