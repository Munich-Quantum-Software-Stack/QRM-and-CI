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
#include "qrmci/Error.h"
#include "qrmci/Runners.h"

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

namespace mqss::qrmci::test {

namespace {

/// @brief A task ready for direct (no_modify) submission -- this file only
///        exercises chooseBackend()'s selection logic, which is independent
///        of the compiler-admission path CircuitFormatPolicy also governs.
mqss::QuantumTask directTask(const std::string &circuit = "OPENQASM 3.0;",
                             std::uint32_t numQubits = 2,
                             std::string type = "qasm3") {
  auto task = makeTask(circuit, numQubits, std::move(type));
  task.set_no_modify(true);
  return task;
}

} // namespace

// ===========================================================================
// chooseBackend
// ===========================================================================
class ChooseBackendTest : public ::testing::Test {
protected:
  mqss::qrmci::BackendRegistry backends;

  void SetUp() override {
    backends.insertOrRefresh(makeBackend("alpha", 5));
    backends.insertOrRefresh(makeBackend("beta", 10));
  }
};

TEST_F(ChooseBackendTest, EmptyRegistryFails) {
  const mqss::qrmci::BackendRegistry empty;
  auto result = mqss::qrmci::chooseBackend(directTask(), empty);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::NoBackendAvailable);
}

TEST_F(ChooseBackendTest, PreferredBackendExists) {
  auto task = directTask("OPENQASM 3.0;", 7);
  task.set_preferred_qpu("beta");
  auto result = mqss::qrmci::chooseBackend(task, backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beta");
}

TEST_F(ChooseBackendTest, PreferredBackendWinsOverThePolicy) {
  // "alpha" would win on both policies; the preferred QPU must override it.
  auto task = directTask();
  task.set_preferred_qpu("beta");
  auto result = mqss::qrmci::chooseBackend(task, backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beta");
}

TEST_F(ChooseBackendTest, IncompatiblePreferredBackendFallsBack) {
  // "alpha" has only 5 qubits, so a 7-qubit task must fall through to the
  // policy rather than failing outright.
  auto task = directTask("OPENQASM 3.0;", 7);
  task.set_preferred_qpu("alpha");
  auto result = mqss::qrmci::chooseBackend(task, backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beta");
}

TEST_F(ChooseBackendTest, UnknownPreferredBackendFallsBack) {
  auto task = directTask();
  task.set_preferred_qpu("does-not-exist");
  auto result = mqss::qrmci::chooseBackend(task, backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "alpha");
}

TEST_F(ChooseBackendTest, TaskRequiresMoreQubitsThanAllBackends) {
  auto result =
      mqss::qrmci::chooseBackend(directTask("OPENQASM 3.0;", 64), backends);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::NoBackendAvailable);
}

TEST_F(ChooseBackendTest, AllBackendsOffline) {
  mqss::qrmci::BackendRegistry offlineOnly;
  offlineOnly.insertOrRefresh(
      makeBackend("offline", 5, mqss::BackendStatus::BACKEND_STATUS_OFFLINE));
  auto task = directTask();
  task.set_preferred_qpu("offline");
  EXPECT_FALSE(mqss::qrmci::chooseBackend(task, offlineOnly).has_value());
}

TEST_F(ChooseBackendTest, OfflinePreferredBackendFallsBackToOnlineBackend) {
  // The preferred QPU's isOnline() check in chooseBackend() exists precisely
  // for this case: an offline preferred backend must not block falling back
  // to a different, online, compatible backend.
  mqss::qrmci::BackendRegistry mixed;
  mixed.insertOrRefresh(
      makeBackend("alpha", 5, mqss::BackendStatus::BACKEND_STATUS_OFFLINE));
  mixed.insertOrRefresh(makeBackend("beta", 5));
  auto task = directTask();
  task.set_preferred_qpu("alpha");
  auto result = mqss::qrmci::chooseBackend(task, mixed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beta");
}

TEST_F(ChooseBackendTest, CircuitFormatUnsupportedByBackends) {
  mqss::qrmci::BackendRegistry qasm2Only;
  qasm2Only.insertOrRefresh(
      makeBackend("qasm2only", 5, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                  {mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2}));
  auto task = directTask("OPENQASM 3.0;", 2, "qasm3");
  task.set_preferred_qpu("qasm2only");
  EXPECT_FALSE(mqss::qrmci::chooseBackend(task, qasm2Only).has_value());
}

TEST_F(ChooseBackendTest, NoPreferredBackendFallsBackToCompatibleBackend) {
  // preferred_qpu is left unset (the empty string, registered under no
  // name), so chooseBackend must fall through to the policy instead of the
  // preferred-backend fast path.
  mqss::qrmci::BackendRegistry singleBackend;
  singleBackend.insertOrRefresh(makeBackend("gamma", 5));
  auto task = directTask();
  ASSERT_TRUE(task.preferred_qpu().empty());
  auto result = mqss::qrmci::chooseBackend(task, singleBackend);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "gamma");
}

TEST_F(ChooseBackendTest, NoPreferredBackendAndNoCompatibleBackendFails) {
  mqss::qrmci::BackendRegistry offlineOnly;
  offlineOnly.insertOrRefresh(
      makeBackend("offline", 5, mqss::BackendStatus::BACKEND_STATUS_OFFLINE));
  auto task = directTask();
  ASSERT_TRUE(task.preferred_qpu().empty());
  auto result = mqss::qrmci::chooseBackend(task, offlineOnly);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::NoBackendAvailable);
}

TEST_F(ChooseBackendTest, RestrictedTaskSkipsBackendsOutsideItsAllowList) {
  auto task = directTask();
  task.add_restricted_resource_names("beta");
  auto result = mqss::qrmci::chooseBackend(task, backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beta");
}

TEST_F(ChooseBackendTest, RestrictedTaskWithNoAllowedBackendFails) {
  auto task = directTask();
  task.add_restricted_resource_names("does-not-exist");
  auto result = mqss::qrmci::chooseBackend(task, backends);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, mqss::qrmci::Error::Kind::NoBackendAvailable);
}

TEST_F(ChooseBackendTest, PreferredBackendOutsideAllowListFallsBack) {
  // "alpha" would otherwise win as the preferred QPU, but the task's
  // allow-list excludes it, so selection must fall through to "beta".
  auto task = directTask();
  task.set_preferred_qpu("alpha");
  task.add_restricted_resource_names("beta");
  auto result = mqss::qrmci::chooseBackend(task, backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beta");
}

TEST_F(ChooseBackendTest, DoesNotWriteThroughTheTask) {
  // chooseBackend is a pure query: the caller assigns the result. Neither
  // the success nor the failure path may touch the task.
  auto task = directTask();
  ASSERT_TRUE(mqss::qrmci::chooseBackend(task, backends).has_value());
  EXPECT_TRUE(task.scheduled_qpu().empty());

  auto oversizedTask = directTask("OPENQASM 3.0;", 64);
  oversizedTask.set_scheduled_qpu("previously-set");
  EXPECT_FALSE(mqss::qrmci::chooseBackend(oversizedTask, backends).has_value());
  EXPECT_EQ(oversizedTask.scheduled_qpu(), "previously-set");
}

// ===========================================================================
// ChooseBackendPolicyTest
// The point of naming the policy: with several equally compatible backends
// the winner is specified, not an accident of hash-map iteration order.
// ===========================================================================
class ChooseBackendPolicyTest : public ::testing::Test {
protected:
  mqss::qrmci::BackendRegistry backends;

  void SetUp() override {
    // Three backends that can all run the fixture task, registered out of
    // name order and with differing qubit counts so the two policies
    // disagree about the winner.
    backends.insertOrRefresh(makeBackend("mu", 12));
    backends.insertOrRefresh(makeBackend("chi", 5));
    backends.insertOrRefresh(makeBackend("psi", 8));
  }
};

TEST_F(ChooseBackendPolicyTest, LowestNameIsTheDefault) {
  auto result = mqss::qrmci::chooseBackend(directTask(), backends);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "chi");
}

TEST_F(ChooseBackendPolicyTest, LowestNameWinsAmongEquallyCompatible) {
  mqss::qrmci::BackendRegistry equalBackends;
  equalBackends.insertOrRefresh(makeBackend("zeta", 5));
  equalBackends.insertOrRefresh(makeBackend("eta", 5));
  auto result = mqss::qrmci::chooseBackend(
      directTask(), equalBackends,
      mqss::qrmci::BackendSelectionPolicy::LowestName);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "eta");
}

TEST_F(ChooseBackendPolicyTest, SmallestSufficientPicksTheSmallestBackend) {
  auto result = mqss::qrmci::chooseBackend(
      directTask(), backends,
      mqss::qrmci::BackendSelectionPolicy::SmallestSufficient);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "chi");
}

TEST_F(ChooseBackendPolicyTest, SmallestSufficientSkipsTooSmallBackends) {
  // "chi" (5 qubits) cannot run a 6-qubit task, so the next smallest that
  // can must win -- not the first one encountered.
  auto result = mqss::qrmci::chooseBackend(
      directTask("OPENQASM 3.0;", 6), backends,
      mqss::qrmci::BackendSelectionPolicy::SmallestSufficient);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "psi");
}

TEST_F(ChooseBackendPolicyTest, SmallestSufficientBreaksTiesByName) {
  mqss::qrmci::BackendRegistry equalBackends;
  equalBackends.insertOrRefresh(makeBackend("zeta", 5));
  equalBackends.insertOrRefresh(makeBackend("eta", 5));
  auto result = mqss::qrmci::chooseBackend(
      directTask(), equalBackends,
      mqss::qrmci::BackendSelectionPolicy::SmallestSufficient);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "eta");
}

TEST_F(ChooseBackendPolicyTest, ChoiceIsStableAcrossInsertionOrders) {
  // Same backends, opposite insertion order: the answer must not move.
  mqss::qrmci::BackendRegistry reversed;
  reversed.insertOrRefresh(makeBackend("psi", 8));
  reversed.insertOrRefresh(makeBackend("chi", 5));
  reversed.insertOrRefresh(makeBackend("mu", 12));

  const auto task = directTask();
  EXPECT_EQ(mqss::qrmci::chooseBackend(task, backends).value(),
            mqss::qrmci::chooseBackend(task, reversed).value());
}

TEST_F(ChooseBackendPolicyTest, RepeatedCallsAgree) {
  const auto task = directTask();
  const auto first = mqss::qrmci::chooseBackend(task, backends);
  ASSERT_TRUE(first.has_value());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(mqss::qrmci::chooseBackend(task, backends).value(), *first);
  }
}

TEST_F(ChooseBackendPolicyTest, OfflineBackendsAreSkippedByThePolicy) {
  mqss::qrmci::BackendRegistry mixed;
  mixed.insertOrRefresh(
      makeBackend("aaa", 5, mqss::BackendStatus::BACKEND_STATUS_OFFLINE));
  mixed.insertOrRefresh(
      makeBackend("bbb", 5, mqss::BackendStatus::BACKEND_STATUS_BUSY));
  auto result = mqss::qrmci::chooseBackend(directTask(), mixed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "bbb");
}

TEST_F(ChooseBackendPolicyTest, ExpiredBackendsAreNotChosen) {
  mqss::qrmci::BackendRegistry shortLived{std::chrono::seconds(1)};
  const mqss::qrmci::BackendRegistry::Clock::time_point epoch{};
  shortLived.insertOrRefresh(makeBackend("alpha", 5), epoch);
  shortLived.insertOrRefresh(makeBackend("beta", 5),
                             epoch + std::chrono::seconds(10));
  shortLived.expire(epoch + std::chrono::seconds(10));

  auto result = mqss::qrmci::chooseBackend(directTask(), shortLived);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "beta");
}

} // namespace mqss::qrmci::test
