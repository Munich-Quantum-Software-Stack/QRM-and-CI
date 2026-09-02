/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/CommunicationHandler.h"

#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"

namespace mqss::qrmci {

namespace {

/// @brief Translate the daemon's RabbitMqConnectionConfig into the RabbitMQ
///        transport's own options.
mqss::TransportOptions<mqss::RabbitMqSimple>
makeTransportOptions(const mqss::qrmci::RabbitMqConnectionConfig &config) {
  mqss::TransportOptions<mqss::RabbitMqSimple> options{};
  options.host = std::string(config.host);
  options.port = config.port;
  options.username = std::string(config.user);
  options.password = std::string(config.password);
  return options;
}

} // namespace

CommunicationHandler::CommunicationHandler(
    const mqss::qrmci::RabbitMqConnectionConfig &config)
    : messenger(makeTransportOptions(config)) {}

} // namespace mqss::qrmci
