/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/CommunicationHandler.h"

#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"
#include "qrmci/Config.h"

namespace mqss::qrmci {

mqss::TransportOptions<mqss::RabbitMqSimple>
makeTransportOptions(const RabbitMqConnectionConfig &config) {
  mqss::TransportOptions<mqss::RabbitMqSimple> options{};
  options.host = config.host;
  options.port = config.port;
  options.username = config.user;
  options.password = config.password;
  options.vhost = config.vhost;
  return options;
}

CommunicationHandler::CommunicationHandler(
    const RabbitMqConnectionConfig &config)
    : messenger(makeTransportOptions(config)) {}

} // namespace mqss::qrmci
