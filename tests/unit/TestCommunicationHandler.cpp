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
#include <concepts>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <type_traits>

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
// Compile-time contracts: no generic messageLabel fallback, no copy/move.
// These are regression assertions with no runtime body -- a change that
// breaks either contract fails to compile this file at all.
// ===========================================================================
namespace {

/// @brief Whether messageLabel(T) resolves to something other than the
///        deleted generic fallback.
template <class T>
concept HasMessageLabel = requires(const T &value) {
  { messageLabel(value) } -> std::same_as<std::string>;
};

/// @brief A type with no messageLabel overload of its own, to prove the
///        generic fallback is actually gone rather than merely unused.
struct UnsupportedMessageType {};

static_assert(HasMessageLabel<mqss::QuantumTask>);
static_assert(HasMessageLabel<mqss::QuantumResult>);
static_assert(HasMessageLabel<mqss::Backend>);
static_assert(!HasMessageLabel<UnsupportedMessageType>);

static_assert(!std::is_copy_constructible_v<CommunicationHandler>);
static_assert(!std::is_copy_assignable_v<CommunicationHandler>);
static_assert(!std::is_move_constructible_v<CommunicationHandler>);
static_assert(!std::is_move_assignable_v<CommunicationHandler>);

} // namespace

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
// MakeTransportOptionsTest
// Exercises the free function that translates a RabbitMqConnectionConfig
// into TransportOptions<RabbitMqSimple> -- the translation step that used
// to silently drop vhost.
// ===========================================================================
TEST(MakeTransportOptionsTest, EveryConfiguredFieldReachesTheTransport) {
  const RabbitMqConnectionConfig cfg{
      .host = "broker",
      .port = 5673,
      .user = "u",
      .password = "p",
      .vhost = "/production",
  };
  const auto options = makeTransportOptions(cfg);
  EXPECT_EQ(options.host, "broker");
  EXPECT_EQ(options.port, 5673);
  EXPECT_EQ(options.username, "u");
  EXPECT_EQ(options.password, "p");
  EXPECT_EQ(options.vhost, "/production");
}

// ===========================================================================
// CommunicationHandlerUnreachableBrokerTest
// Points the handler at 127.0.0.1:5673, a port nothing listens on in this
// environment. RabbitMqSimpleTransport::send()/receive() open their channel
// lazily and catch every SimpleAmqpClient exception, converting connection
// failure into Status::unavailable(...). That lets us deterministically drive
// CommunicationHandler's error reporting against the real transport, and to
// confirm that a broker failure comes back as a std::expected the caller can
// log and carry on from rather than as an exception.
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
  EXPECT_TRUE(contains(received.error().detail, "Receive error"))
      << received.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       ReceiveQuantumResultReportsErrorOnUnreachableBroker) {
  auto received = handler.receive<mqss::QuantumResult>(
      "some-queue", std::chrono::milliseconds(200));
  ASSERT_FALSE(received.has_value());
  EXPECT_EQ(received.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
  EXPECT_TRUE(contains(received.error().detail, "Receive error"))
      << received.error().detail;
}

TEST_F(CommunicationHandlerUnreachableBrokerTest,
       ReceiveBackendStatusReportsErrorOnUnreachableBroker) {
  auto received = handler.receive<mqss::Backend>(
      "some-queue", std::chrono::milliseconds(200));
  ASSERT_FALSE(received.has_value());
  EXPECT_EQ(received.error().kind, mqss::qrmci::Error::Kind::MessagingFailed);
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
  // A set flag must short-circuit at every timeout, not just at 0ms: if it
  // only applied to the non-blocking poll, this would reach the broker and
  // come back as an error instead.
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
      queue, mqss::qrmci::WaitForever, terminationFlag);
  setter.join();

  ASSERT_TRUE(received.has_value()) << received.error().detail;
  EXPECT_FALSE(received->has_value());
}

TEST_F(CommunicationHandlerLiveBrokerTest,
       ZeroTimeoutPollsOnceAndReturnsImmediatelyWhenEmpty) {
  const std::string queue = "test.communicationhandler.pollonce";
  std::atomic<bool> terminationFlag{false};

  const auto start = std::chrono::steady_clock::now();
  auto received = handler.receive<mqss::QuantumTask>(
      queue, std::chrono::milliseconds(0), terminationFlag);
  const auto elapsed = std::chrono::steady_clock::now() - start;

  ASSERT_TRUE(received.has_value()) << received.error().detail;
  EXPECT_FALSE(received->has_value());
  // A single non-blocking poll, not DefaultReceiveTimeout's 500ms slice --
  // generous margin for scheduling jitter and the broker round trip.
  EXPECT_LT(elapsed, std::chrono::milliseconds(300));
}

} // namespace mqss::qrmci::test
