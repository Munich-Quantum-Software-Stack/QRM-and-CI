/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "RunnersTestHelpers.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"

#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

namespace mqss::qrmci::test {

using Clock = mqss::qrmci::BackendRegistry::Clock;

namespace {
// A fixed epoch to build test timestamps from, so no test depends on how
// long it takes to run.
const Clock::time_point Epoch{};
} // namespace

// ===========================================================================
// BackendRegistryInsertTest
// ===========================================================================
class BackendRegistryInsertTest : public ::testing::Test {
protected:
  mqss::qrmci::BackendRegistry registry;
};

TEST_F(BackendRegistryInsertTest, StartsEmpty) {
  EXPECT_TRUE(registry.empty());
  EXPECT_EQ(registry.size(), 0U);
}

TEST_F(BackendRegistryInsertTest, InsertMakesBackendFindable) {
  registry.insertOrRefresh(makeBackend("alpha", 5), Epoch);
  const auto *found = registry.find("alpha");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getName(), "alpha");
  EXPECT_EQ(found->getNumQubits(), 5U);
}

TEST_F(BackendRegistryInsertTest, ContainsMatchesFind) {
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  EXPECT_TRUE(registry.contains("alpha"));
  EXPECT_FALSE(registry.contains("beta"));
}

TEST_F(BackendRegistryInsertTest, FindOnUnknownNameReturnsNull) {
  // The bug this replaces: operator[] on a raw map default-inserted an empty
  // BackendWrapper on a miss. A miss must be a null return and must not grow
  // the registry.
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  EXPECT_EQ(registry.find("nonexistent"), nullptr);
  EXPECT_EQ(registry.size(), 1U);
}

TEST_F(BackendRegistryInsertTest, FindOnEmptyNameReturnsNull) {
  // A task whose scheduled QPU was never set looks up the empty string.
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  EXPECT_EQ(registry.find(""), nullptr);
  EXPECT_EQ(registry.size(), 1U);
}

TEST_F(BackendRegistryInsertTest, InsertingSameNameRefreshesInPlace) {
  registry.insertOrRefresh(makeBackend("alpha", 5), Epoch);
  registry.insertOrRefresh(makeBackend("alpha", 9), Epoch);
  EXPECT_EQ(registry.size(), 1U);
  ASSERT_NE(registry.find("alpha"), nullptr);
  EXPECT_EQ(registry.find("alpha")->getNumQubits(), 9U);
}

TEST_F(BackendRegistryInsertTest, DistinctNamesCoexist) {
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  registry.insertOrRefresh(makeBackend("beta"), Epoch);
  EXPECT_EQ(registry.size(), 2U);
}

TEST_F(BackendRegistryInsertTest, ForEachVisitsBackendsInNameOrder) {
  registry.insertOrRefresh(makeBackend("gamma"), Epoch);
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  registry.insertOrRefresh(makeBackend("beta"), Epoch);

  std::vector<std::string> visited;
  registry.forEachBackend(
      [&visited](const std::string &name,
                 const mqss::qrmci::BackendWrapper & /*backend*/) {
        visited.push_back(name);
      });
  const std::vector<std::string> expected = {"alpha", "beta", "gamma"};
  EXPECT_EQ(visited, expected);
}

// ===========================================================================
// BackendRegistryExpiryTest
// ===========================================================================
class BackendRegistryExpiryTest : public ::testing::Test {
protected:
  static constexpr auto TimeToLive = std::chrono::seconds(30);
  static constexpr auto PublishInterval = std::chrono::seconds(5);
  mqss::qrmci::BackendRegistry registry{TimeToLive, PublishInterval};
};

TEST_F(BackendRegistryExpiryTest, FreshEntrySurvives) {
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  EXPECT_EQ(registry.expire(Epoch + TimeToLive), 0U);
  EXPECT_TRUE(registry.contains("alpha"));
}

TEST_F(BackendRegistryExpiryTest, StaleEntryIsDropped) {
  // A worker that dies stops publishing its status; its backend must stop
  // receiving task assignments instead of lingering in the registry forever.
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  EXPECT_EQ(registry.expire(Epoch + TimeToLive + std::chrono::seconds(1)), 1U);
  EXPECT_FALSE(registry.contains("alpha"));
  EXPECT_TRUE(registry.empty());
}

TEST_F(BackendRegistryExpiryTest, RefreshResetsTheClock) {
  registry.insertOrRefresh(makeBackend("alpha"), Epoch);
  registry.insertOrRefresh(makeBackend("alpha"), Epoch + TimeToLive);
  EXPECT_EQ(registry.expire(Epoch + TimeToLive + std::chrono::seconds(1)), 0U);
  EXPECT_TRUE(registry.contains("alpha"));
}

TEST_F(BackendRegistryExpiryTest, OnlyStaleEntriesAreDropped) {
  registry.insertOrRefresh(makeBackend("stale"), Epoch);
  registry.insertOrRefresh(makeBackend("fresh"), Epoch + TimeToLive);
  EXPECT_EQ(registry.expire(Epoch + TimeToLive + std::chrono::seconds(1)), 1U);
  EXPECT_FALSE(registry.contains("stale"));
  EXPECT_TRUE(registry.contains("fresh"));
}

TEST_F(BackendRegistryExpiryTest, ExpireOnEmptyRegistryIsANoOp) {
  EXPECT_EQ(registry.expire(Epoch + std::chrono::hours(1)), 0U);
}

TEST_F(BackendRegistryExpiryTest, DefaultTimeToLiveOutlivesPublishInterval) {
  // A process that refreshes its own entry on the publish interval must not
  // be able to expire itself in between.
  const mqss::qrmci::BackendRegistry defaults;
  EXPECT_GT(defaults.getTimeToLive(), defaults.getPublishInterval());
}

// ===========================================================================
// BackendRegistryPublishSlotTest
// ===========================================================================
class BackendRegistryPublishSlotTest : public ::testing::Test {
protected:
  static constexpr auto TimeToLive = std::chrono::seconds(30);
  static constexpr auto PublishInterval = std::chrono::seconds(5);
  mqss::qrmci::BackendRegistry registry{TimeToLive, PublishInterval};
};

TEST_F(BackendRegistryPublishSlotTest, FirstClaimSucceeds) {
  EXPECT_TRUE(registry.claimPublishSlot(Epoch));
}

TEST_F(BackendRegistryPublishSlotTest, SecondClaimWithinIntervalFails) {
  // This is what decouples publication from the work loop: a loop spinning
  // ten times a second must not produce ten status publications a second.
  EXPECT_TRUE(registry.claimPublishSlot(Epoch));
  EXPECT_FALSE(
      registry.claimPublishSlot(Epoch + std::chrono::milliseconds(100)));
  EXPECT_FALSE(registry.claimPublishSlot(Epoch + PublishInterval -
                                         std::chrono::milliseconds(1)));
}

TEST_F(BackendRegistryPublishSlotTest, ClaimSucceedsAgainAfterInterval) {
  EXPECT_TRUE(registry.claimPublishSlot(Epoch));
  EXPECT_TRUE(registry.claimPublishSlot(Epoch + PublishInterval));
}

TEST_F(BackendRegistryPublishSlotTest, ClaimingIsPacedFromTheLastClaim) {
  // The clock restarts at each successful claim, not at fixed multiples of
  // the interval from the first one.
  EXPECT_TRUE(registry.claimPublishSlot(Epoch));
  EXPECT_TRUE(registry.claimPublishSlot(Epoch + PublishInterval));
  EXPECT_FALSE(registry.claimPublishSlot(Epoch + PublishInterval +
                                         std::chrono::seconds(1)));
  EXPECT_TRUE(
      registry.claimPublishSlot(Epoch + PublishInterval + PublishInterval));
}

} // namespace mqss::qrmci::test
