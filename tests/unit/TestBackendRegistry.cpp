/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "BackendWrapperTestBuilder.h"
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

TEST_F(BackendRegistryInsertTest, InsertReturnsTrueOnSuccess) {
  EXPECT_TRUE(registry.insertOrRefresh(makeBackend("alpha"), Epoch));
}

TEST_F(BackendRegistryInsertTest, SameQueueRefreshSucceeds) {
  EXPECT_TRUE(registry.insertOrRefresh(
      BackendBuilder().name("alpha").queueName("q1").build(), Epoch));
  EXPECT_TRUE(registry.insertOrRefresh(
      BackendBuilder().name("alpha").numQubits(9).queueName("q1").build(),
      Epoch));
  ASSERT_NE(registry.find("alpha"), nullptr);
  EXPECT_EQ(registry.find("alpha")->getNumQubits(), 9U);
}

TEST_F(BackendRegistryInsertTest,
       ConflictingQueueIsRejectedAndLeavesTheExistingEntry) {
  // The same backend ID publishing under two different dispatch queues is a
  // routing conflict a task could silently be sent to the wrong worker
  // over, so the conflicting status must be rejected rather than applied.
  ASSERT_TRUE(registry.insertOrRefresh(
      BackendBuilder().name("alpha").numQubits(5).queueName("q1").build(),
      Epoch));
  EXPECT_FALSE(registry.insertOrRefresh(
      BackendBuilder().name("alpha").numQubits(9).queueName("q2").build(),
      Epoch));

  ASSERT_NE(registry.find("alpha"), nullptr);
  EXPECT_EQ(registry.find("alpha")->getNumQubits(), 5U);
  EXPECT_EQ(registry.find("alpha")->getQueueName(), "q1");
}

TEST_F(BackendRegistryInsertTest, EmptyQueueNeverConflicts) {
  // An entry with no dispatch queue of its own (a self-registering process
  // that never publishes) has nothing to conflict over.
  ASSERT_TRUE(registry.insertOrRefresh(
      BackendBuilder().name("alpha").numQubits(5).build(), Epoch));
  EXPECT_TRUE(registry.insertOrRefresh(
      BackendBuilder().name("alpha").numQubits(9).queueName("q1").build(),
      Epoch));
  ASSERT_NE(registry.find("alpha"), nullptr);
  EXPECT_EQ(registry.find("alpha")->getNumQubits(), 9U);
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
  mqss::qrmci::BackendRegistry registry{TimeToLive};
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

} // namespace mqss::qrmci::test
