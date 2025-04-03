/**
 * @file QuantumResourceManager.hpp
 * @brief TODO
 */
#pragma once

#include "ConnectionHandler.hpp"
#include "mqss/common/QuantumTask.hpp"

/**
 * @todo Document this
 */

using namespace mqss;

void handleQuantumDaemon(amqp_connection_state_t &conn, char const *QDQueue,
                         const QuantumTask &quantumTask);
void signalHandler(int signum);
