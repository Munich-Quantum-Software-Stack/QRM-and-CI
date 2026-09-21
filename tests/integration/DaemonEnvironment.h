/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file DaemonEnvironment.h
/// @brief GTest global environment that owns the daemon(s) a
///        tests/integration binary needs for the duration of the run.

#pragma once

#include "DaemonProcess.h"
#include "qrmci/Config.h"

#include <chrono>
#include <gtest/gtest.h>
#include <vector>

namespace mqss::qrmci::test {

/// @brief Starts every given DaemonProcess in SetUp(), waits until RabbitMQ
///        is actually reachable, and stops every daemon in reverse start
///        order in TearDown(). One instance covers both the one-daemon
///        (standalone) and two-daemon (distributed selector+worker) cases --
///        the readiness/start/teardown logic is identical either way.
///
/// Readiness is a real send()/receive() round-trip through the broker, not
/// just constructing a CommunicationHandler: CommunicationHandler's
/// constructor never fails, because the underlying transport
/// (RabbitMqSimpleTransport, vendored from MQSS-Integration-Deployment-
/// Framework) makes channel creation lazy on purpose, precisely so it never
/// throws before a Status can be returned. The round trip mostly proves the
/// broker is reachable and no daemon has already crashed -- not that a
/// daemon has finished loading its QDMI device and is polling its own
/// queue. That residual gap is left to each TEST_F's own bounded receive()
/// retrying across a generous budget.
class DaemonEnvironment : public ::testing::Environment {
public:
  /// @param extraReadinessGrace Extra time to wait, after the broker
  ///        round-trip first succeeds, before returning from SetUp(). The
  ///        round trip only proves the broker is reachable; a distributed
  ///        worker's own backend-status publish to the selector (governed
  ///        by SubmitterConfig::backendStatusPublishInterval, 5s by default)
  ///        can still be outstanding, and a task submitted before the
  ///        selector's registry has anything in it is legitimately
  ///        cancelled for lacking a suitable backend rather than merely
  ///        delayed -- so a bounded retry on the result alone cannot paper
  ///        over this gap the way it does for a slow-arriving result.
  DaemonEnvironment(std::vector<DaemonProcess> daemons,
                    mqss::qrmci::RabbitMqConnectionConfig connection,
                    std::chrono::milliseconds extraReadinessGrace =
                        std::chrono::milliseconds{0});

  void SetUp() override;
  void TearDown() override;

private:
  std::vector<DaemonProcess> daemons;
  mqss::qrmci::RabbitMqConnectionConfig connection;
  std::chrono::milliseconds extraReadinessGrace;
};

} // namespace mqss::qrmci::test
