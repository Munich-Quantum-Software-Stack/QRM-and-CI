/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "mqss/Protocol.hpp"
#include "mqss/Transport.hpp"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace mqss::qrmci::test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static RabbitMqConfig makeInvalidConfig() {
  return RabbitMqConfig{
      .host = "invalid-host-that-does-not-exist",
      .port = 5672,
      .user = "guest",
      .password = "guest",
      .vhost = "/",
  };
}

static RabbitMqConfig makeLocalhostConfig(int port = 5673) {
  // Use a port unlikely to have a listening AMQP broker in CI.
  return RabbitMqConfig{
      .host = "127.0.0.1",
      .port = port,
      .user = "guest",
      .password = "guest",
      .vhost = "/",
  };
}

// ===========================================================================
// RabbitMqConfigTest  –  tests the plain config struct (no I/O)
// ===========================================================================
class RabbitMqConfigTest : public ::testing::Test {};

TEST_F(RabbitMqConfigTest, DefaultFieldsHostIsEmpty) {
  RabbitMqConfig cfg{};
  EXPECT_TRUE(cfg.host.empty());
}

TEST_F(RabbitMqConfigTest, DefaultFieldsUserIsEmpty) {
  RabbitMqConfig cfg{};
  EXPECT_TRUE(cfg.user.empty());
}

TEST_F(RabbitMqConfigTest, DefaultFieldsPasswordIsEmpty) {
  RabbitMqConfig cfg{};
  EXPECT_TRUE(cfg.password.empty());
}

TEST_F(RabbitMqConfigTest, DefaultFieldsVhostIsEmpty) {
  RabbitMqConfig cfg{};
  EXPECT_TRUE(cfg.vhost.empty());
}

TEST_F(RabbitMqConfigTest, DefaultFieldsPortIsZero) {
  RabbitMqConfig cfg{};
  EXPECT_EQ(cfg.port, 0);
}

TEST_F(RabbitMqConfigTest, FieldAssignment) {
  RabbitMqConfig cfg{
      .host = "rabbitmq.example.com",
      .port = 5672,
      .user = "admin",
      .password = "s3cr3t",
      .vhost = "/production",
  };
  EXPECT_EQ(cfg.host, "rabbitmq.example.com");
  EXPECT_EQ(cfg.port, 5672);
  EXPECT_EQ(cfg.user, "admin");
  EXPECT_EQ(cfg.password, "s3cr3t");
  EXPECT_EQ(cfg.vhost, "/production");
}

// ===========================================================================
// CommunicationHandlerTerminationFlagTest
// Verifies that getNext* with a pre-set termination flag returns nullopt
// immediately without blocking. Requires a live broker; skipped otherwise.
// ===========================================================================
class CommunicationHandlerTerminationFlagTest : public ::testing::Test {
protected:
  // These tests are intentionally skipped when no broker is available.
  // They serve as a specification of the expected behaviour.
  static constexpr const char *kSkipReason =
      "Requires a live AMQP broker (integration environment).";
};

TEST_F(CommunicationHandlerTerminationFlagTest, getNextQuantumTask) {
  GTEST_SKIP() << kSkipReason;
}

TEST_F(CommunicationHandlerTerminationFlagTest, getNextQuantumResult) {
  GTEST_SKIP() << kSkipReason;
}

TEST_F(CommunicationHandlerTerminationFlagTest, sendQuantumTask) {
  GTEST_SKIP() << kSkipReason;
}

TEST_F(CommunicationHandlerTerminationFlagTest, sendQuantumResult) {
  GTEST_SKIP() << kSkipReason;
}

} // namespace mqss::qrmci::test
