/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "DaemonEnvironment.h"

#include "qrmci/CommunicationHandler.h"

#include <chrono>
#include <thread>
#include <utility>

namespace mqss::qrmci::test {

namespace {

constexpr std::chrono::seconds ReadinessTimeout{30};
constexpr std::chrono::milliseconds ProbeRetryInterval{200};
constexpr std::chrono::milliseconds ProbeReceiveTimeout{1000};

// A dedicated queue, distinct from every queue a real test uses, so the
// probe's own traffic can never be mistaken for (or interfere with) a
// test's own messages. Only one of the daemon-owning tests/integration
// targets runs at a time (RESOURCE_LOCK in CMakeLists.txt), so a fixed name
// is safe.
const std::string &probeQueueName() {
  static const std::string name = "qrmci.integration_test.daemon_probe";
  return name;
}

// The only way to actually observe broker reachability: CommunicationHandler
// never fails to construct (see DaemonEnvironment.h), so a real
// publish-then-consume round trip is required instead.
bool probeBrokerReachable(
    const mqss::qrmci::RabbitMqConnectionConfig &connection) {
  mqss::qrmci::CommunicationHandler handler(connection);

  mqss::QuantumTask probeTask;
  probeTask.set_task_id(-1);
  if (!handler.send(probeTask, probeQueueName())) {
    return false;
  }

  auto received =
      handler.receive<mqss::QuantumTask>(probeQueueName(), ProbeReceiveTimeout);
  return received.has_value() && received->has_value();
}

} // namespace

DaemonEnvironment::DaemonEnvironment(
    std::vector<DaemonProcess> daemons,
    mqss::qrmci::RabbitMqConnectionConfig connection,
    std::chrono::milliseconds extraReadinessGrace)
    : daemons(std::move(daemons)), connection(std::move(connection)),
      extraReadinessGrace(extraReadinessGrace) {}

void DaemonEnvironment::SetUp() {
  for (auto &daemon : daemons) {
    ASSERT_TRUE(daemon.start()) << "Failed to start " << daemon.getLabel();
  }

  const auto deadline = std::chrono::steady_clock::now() + ReadinessTimeout;
  while (std::chrono::steady_clock::now() < deadline) {
    for (auto &daemon : daemons) {
      ASSERT_TRUE(daemon.isRunning())
          << daemon.getLabel() << " exited before becoming ready; see "
          << daemon.getStdioLogPath();
    }

    if (probeBrokerReachable(connection)) {
      if (extraReadinessGrace.count() > 0) {
        std::this_thread::sleep_for(extraReadinessGrace);
      }
      return;
    }

    std::this_thread::sleep_for(ProbeRetryInterval);
  }

  FAIL() << "Timed out after " << ReadinessTimeout.count()
         << "s waiting for RabbitMQ to become reachable.";
}

void DaemonEnvironment::TearDown() {
  for (auto it = daemons.rbegin(); it != daemons.rend(); ++it) {
    it->stop();
  }
}

} // namespace mqss::qrmci::test
