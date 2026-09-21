/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/BackendRegistry.h"
#include "qrmci/PublicationThrottle.h"

#include <chrono>
#include <gtest/gtest.h>

namespace mqss::qrmci::test {

using Clock = mqss::qrmci::PublicationThrottle::Clock;

namespace {
// A fixed epoch to build test timestamps from, so no test depends on how
// long it takes to run.
const Clock::time_point Epoch{};
} // namespace

// ===========================================================================
// PublicationThrottleTest
// ===========================================================================
class PublicationThrottleTest : public ::testing::Test {
protected:
  static constexpr auto Interval = std::chrono::seconds(5);
  mqss::qrmci::PublicationThrottle throttle{Interval};
};

TEST_F(PublicationThrottleTest, FirstClaimSucceeds) {
  EXPECT_TRUE(throttle.claim(Epoch));
}

TEST_F(PublicationThrottleTest, SecondClaimWithinIntervalFails) {
  // This is what decouples publication from the work loop: a loop spinning
  // ten times a second must not produce ten status publications a second.
  EXPECT_TRUE(throttle.claim(Epoch));
  EXPECT_FALSE(throttle.claim(Epoch + std::chrono::milliseconds(100)));
  EXPECT_FALSE(throttle.claim(Epoch + Interval - std::chrono::milliseconds(1)));
}

TEST_F(PublicationThrottleTest, ClaimSucceedsAgainAfterInterval) {
  EXPECT_TRUE(throttle.claim(Epoch));
  EXPECT_TRUE(throttle.claim(Epoch + Interval));
}

TEST_F(PublicationThrottleTest, ClaimingIsPacedFromTheLastClaim) {
  // The clock restarts at each successful claim, not at fixed multiples of
  // the interval from the first one.
  EXPECT_TRUE(throttle.claim(Epoch));
  EXPECT_TRUE(throttle.claim(Epoch + Interval));
  EXPECT_FALSE(throttle.claim(Epoch + Interval + std::chrono::seconds(1)));
  EXPECT_TRUE(throttle.claim(Epoch + Interval + Interval));
}

TEST_F(PublicationThrottleTest, GetIntervalReturnsTheConfiguredInterval) {
  EXPECT_EQ(throttle.getInterval(), Interval);
}

TEST(PublicationThrottleDefaultsTest,
     DefaultIntervalIsShorterThanTheRegistryDefaultTimeToLive) {
  // A process that refreshes its own entry on the default publish interval
  // must not be able to expire itself in between, when both defaults are
  // used together.
  EXPECT_GT(mqss::qrmci::BackendRegistry::DefaultTimeToLive,
            mqss::qrmci::PublicationThrottle::DefaultInterval);
}

} // namespace mqss::qrmci::test
