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
#include "qrmci/Error.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <thread>

namespace mqss::qrmci::test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static RabbitMqConnectionConfig makeInvalidConfig() {
  return RabbitMqConnectionConfig{
      .host = "invalid-host-that-does-not-exist",
      .port = 5672,
      .user = "guest",
      .password = "guest",
      .vhost = "/",
  };
}

static RabbitMqConnectionConfig makeLocalhostConfig(int port = 5673) {
  // Use a port unlikely to have a listening AMQP broker in CI.
  return RabbitMqConnectionConfig{
      .host = "127.0.0.1",
      .port = port,
      .user = "guest",
      .password = "guest",
      .vhost = "/",
  };
}

static bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

// ===========================================================================
// RabbitMqConnectionConfigTest  –  tests the plain config struct (no I/O)
// ===========================================================================
class RabbitMqConnectionConfigTest : public ::testing::Test {};

TEST_F(RabbitMqConnectionConfigTest, DefaultFieldsHostIsEmpty) {
  RabbitMqConnectionConfig cfg{};
  EXPECT_TRUE(cfg.host.empty());
}

TEST_F(RabbitMqConnectionConfigTest, DefaultFieldsUserIsEmpty) {
  RabbitMqConnectionConfig cfg{};
  EXPECT_TRUE(cfg.user.empty());
}

TEST_F(RabbitMqConnectionConfigTest, DefaultFieldsPasswordIsEmpty) {
  RabbitMqConnectionConfig cfg{};
  EXPECT_TRUE(cfg.password.empty());
}

TEST_F(RabbitMqConnectionConfigTest, DefaultFieldsVhostIsEmpty) {
  RabbitMqConnectionConfig cfg{};
  EXPECT_TRUE(cfg.vhost.empty());
}

TEST_F(RabbitMqConnectionConfigTest, DefaultFieldsPortIsZero) {
  RabbitMqConnectionConfig cfg{};
  EXPECT_EQ(cfg.port, 0);
}

TEST_F(RabbitMqConnectionConfigTest, FieldAssignment) {
  RabbitMqConnectionConfig cfg{
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
// CommunicationHandlerUnreachableBrokerTest
// Points the handler at 127.0.0.1:5673, a port nothing listens on in this
// environment. RabbitMqSimpleTransport::send()/receive() open their channel
// lazily and catch every SimpleAmqpClient exception, converting connection
// failure into Status::unavailable(...). That lets us deterministically drive
// CommunicationHandler's error reporting against the real transport: these
// operations used to throw std::runtime_error straight out of a daemon's
// main(); they now return a std::expected the caller can log and carry on
// from.
// ===========================================================================
using Handler = mqss::qrmci::CommunicationHandler;

class CommunicationHandlerUnreachableBrokerTest : public ::testing::Test {
protected:
  Handler handler{makeLocalhostConfig()};
};

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       SendQuantumTaskReportsErrorOnUnreachableBroker) {
  mqss::QuantumTask task;
  task.set_task_id(7);
  auto sent = handler.send(task, "some-queue");
  ASSERT_FALSE(sent.has_value());
  EXPECT_EQ(sent.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
  // A broker that is down now may be up on the next turn of the loop, so a
  // transport failure is retryable -- which is what lets a daemon keep its
  // in-flight work instead of rejecting it.
  EXPECT_TRUE(sent.error().isRetryable());
  EXPECT_TRUE(contains(sent.error().detail, "Failed to send quantum task 7"))
      << sent.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       SendQuantumResultReportsErrorOnUnreachableBroker) {
  mqss::QuantumResult result;
  result.set_task_id(7);
  auto sent = handler.send(result, "some-queue");
  ASSERT_FALSE(sent.has_value());
  EXPECT_EQ(sent.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
  EXPECT_TRUE(
      contains(sent.error().detail, "Failed to send quantum result for task 7"))
      << sent.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       SendBackendStatusReportsErrorOnUnreachableBroker) {
  mqss::Backend backend;
  backend.set_name("fake-backend");
  auto sent = handler.send(backend, "some-queue");
  ASSERT_FALSE(sent.has_value());
  EXPECT_EQ(sent.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
  EXPECT_TRUE(contains(sent.error().detail,
                       "Failed to send backend status for backend "
                       "fake-backend"))
      << sent.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       ReceiveQuantumTaskReportsErrorOnUnreachableBroker) {
  // A non-Timeout failure (connection refused -> Status::unavailable) must be
  // reported rather than being swallowed like a timeout.
  auto received = handler.receive<mqss::QuantumTask>(
      "some-queue", std::chrono::milliseconds(200));
  ASSERT_FALSE(received.has_value());
  EXPECT_EQ(received.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
  EXPECT_TRUE(received.error().isRetryable());
  EXPECT_TRUE(contains(received.error().detail, "Receive error"))
      << received.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       ReceiveQuantumResultReportsErrorOnUnreachableBroker) {
  auto received = handler.receive<mqss::QuantumResult>(
      "some-queue", std::chrono::milliseconds(200));
  ASSERT_FALSE(received.has_value());
  EXPECT_EQ(received.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
  EXPECT_TRUE(received.error().isRetryable());
  EXPECT_TRUE(contains(received.error().detail, "Receive error"))
      << received.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       ReceiveBackendStatusReportsErrorOnUnreachableBroker) {
  auto received = handler.receive<mqss::Backend>(
      "some-queue", std::chrono::milliseconds(200));
  ASSERT_FALSE(received.has_value());
  EXPECT_EQ(received.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
  EXPECT_TRUE(received.error().isRetryable());
  EXPECT_TRUE(contains(received.error().detail, "Receive error"))
      << received.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       PreSetTerminationFlagReturnsBeforeTouchingTheBrokerAtZeroTimeout) {
  // The flag is consulted first, so this returns an empty optional rather
  // than the broker error the other receive tests above get.
  const std::atomic<bool> terminationFlag{true};
  auto received = handler.receive<mqss::QuantumTask>(
      "some-queue", std::chrono::milliseconds(0), terminationFlag);
  ASSERT_TRUE(received.has_value()) << received.error().detail;
  EXPECT_FALSE(received->has_value());
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       PreSetTerminationFlagReturnsBeforeTouchingTheBrokerAtNonZeroTimeout) {
  // The regression: with a non-zero timeout the flag used to be inert, so
  // this reached the broker and came back as an error instead.
  const std::atomic<bool> terminationFlag{true};
  auto received = handler.receive<mqss::QuantumTask>(
      "some-queue", std::chrono::milliseconds(500), terminationFlag);
  ASSERT_TRUE(received.has_value()) << received.error().detail;
  EXPECT_FALSE(received->has_value());
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       ConstructionAgainstAnUnresolvableHostDoesNotThrow) {
  EXPECT_NO_THROW({ Handler unusable{makeInvalidConfig()}; });
}

// ===========================================================================
// CommunicationHandlerLiveBrokerTest
// The happy path (a successful send/receive round trip) and the
// terminationFlag-driven polling loop only manifest once receive() actually
// returns Status::timeout(...) repeatedly, which requires a real reachable
// AMQP broker (an unreachable one fails with Status::unavailable(...)
// instead, exercised above). Skipped outside an integration environment with
// a broker at 127.0.0.1:5672.
// ===========================================================================
class CommunicationHandlerLiveBrokerTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (std::getenv("QRMCI_TEST_AMQP_HOST") == nullptr) {
      GTEST_SKIP()
          << "QRMCI_TEST_AMQP_HOST not set; skipping live broker tests";
    }
  }

  static RabbitMqConnectionConfig config() {
    const char *host = std::getenv("QRMCI_TEST_AMQP_HOST");
    return RabbitMqConnectionConfig{
        .host = host != nullptr ? host : "127.0.0.1",
        .port = 5672,
        .user = "guest",
        .password = "guest",
        .vhost = "/",
    };
  }
  Handler handler{config()};
};

TEST_F(CommunicationHandlerLiveBrokerTest, RoundTripSendAndReceiveQuantumTask) {
  mqss::QuantumTask task;
  task.set_task_id(99);
  const std::string queue = "test.communicationhandler.roundtrip";

  auto sent = handler.send(task, queue);
  ASSERT_TRUE(sent.has_value()) << sent.error().detail;
  auto received = handler.receive<mqss::QuantumTask>(
      queue, std::chrono::milliseconds(2000));
  ASSERT_TRUE(received.has_value()) << received.error().detail;
  ASSERT_TRUE(received->has_value());
  EXPECT_EQ((*received)->task_id(), 99);
}

TEST_F(CommunicationHandlerLiveBrokerTest,
       ReceiveQuantumTaskReturnsNulloptWhenTerminationFlagSet) {
  const std::string queue = "test.communicationhandler.terminationflag";
  std::atomic<bool> terminationFlag{false};

  std::thread setter([&terminationFlag] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    terminationFlag.store(true, std::memory_order_release);
  });
  auto received = handler.receive<mqss::QuantumTask>(
      queue, std::chrono::milliseconds(0), terminationFlag);
  setter.join();

  ASSERT_TRUE(received.has_value()) << received.error().detail;
  EXPECT_FALSE(received->has_value());
}

} // namespace mqss::qrmci::test
