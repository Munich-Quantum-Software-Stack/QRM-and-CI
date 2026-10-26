/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "RunnersTestHelpers.h"
#include "fixtures/InstrumentedScenarioControl.h"
#include "qrmci/Error.h"
#include "qrmci/Runners.h"

#include <gtest/gtest.h>
#include <mqss/submitter/Client.h>
#include <mqss/submitter/Device.h>
#include <mqss/submitter/Job.h>
#include <string>

namespace mqss::qrmci::test {
namespace {

using mqss::submitter::Workflow;
using mqss::submitter::testing::fixtures::kInstrumentedDriverPath;
using mqss::submitter::testing::fixtures::scenarioWithDevices;
using mqss::submitter::testing::fixtures::ScopedInstrumentedScenario;
using mqss::submitter::testing::fixtures::setChildren;

TEST(RunnersPlacement, SubmitsThroughChildBatch) {
  auto scenario = scenarioWithDevices("1.3.2", {"parent", "first", "second"});
  setChildren(scenario, 0, {1, 2});
  const ScopedInstrumentedScenario installed{scenario};
  ASSERT_TRUE(installed.loaded());
  mqss::submitter::Client client;
  const auto device =
      client.openDevice(kInstrumentedDriverPath, "parent", {}, {});
  ASSERT_TRUE(device.has_value()) << device.error().message();

  const auto jobs =
      submitQuantumTask(makeTask("OPENQASM 2.0;", 2, "qasm2"), *device);

  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  ASSERT_EQ(jobs->size(), 1U);
  EXPECT_EQ(jobs->front().workflow(), Workflow::ChildBatch);
}

TEST(RunnersPlacement, ChildFailureDoesNotTryParentOrAnotherChild) {
  auto scenario = scenarioWithDevices("1.3.2", {"parent", "second", "first"});
  // The driver's child order, not DEVICES order or name order, selects first.
  setChildren(scenario, 0, {2, 1});
  const ScopedInstrumentedScenario installed{scenario};
  ASSERT_TRUE(installed.loaded());
  mqss::submitter::Client client;
  const auto device =
      client.openDevice(kInstrumentedDriverPath, "parent", {}, {});
  ASSERT_TRUE(device.has_value()) << device.error().message();

  const auto jobs = submitQuantumTask(makeTask("", 2, "qasm2"), *device);

  ASSERT_FALSE(jobs.has_value());
  EXPECT_EQ(jobs.error().kind, Error::Kind::SubmissionFailed);
  EXPECT_NE(jobs.error().detail.find("ChildStandalone on first"),
            std::string::npos);
  EXPECT_EQ(jobs.error().detail.find("Standalone on parent"),
            std::string::npos);
  EXPECT_EQ(jobs.error().detail.find("on second"), std::string::npos);
  EXPECT_EQ(installed.counters().jobSubmitCount, 0);
}

TEST(RunnersPlacement, BatchRefusalStillAllowsStandaloneOnChosenChild) {
  auto scenario = scenarioWithDevices("1.3.2", {"parent", "first"});
  setChildren(scenario, 0, {1});
  scenario.refusesBatchJobFormat = true;
  const ScopedInstrumentedScenario installed{scenario};
  ASSERT_TRUE(installed.loaded());
  mqss::submitter::Client client;
  const auto device =
      client.openDevice(kInstrumentedDriverPath, "parent", {}, {});
  ASSERT_TRUE(device.has_value()) << device.error().message();

  const auto jobs =
      submitQuantumTask(makeTask("OPENQASM 2.0;", 2, "qasm2"), *device);

  ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
  ASSERT_EQ(jobs->size(), 1U);
  EXPECT_EQ(jobs->front().workflow(), Workflow::ChildStandalone);
}

TEST(RunnersPlacement, ChildlessDeviceSubmitsOnSelf) {
  for (const auto *version : {"1.3.0", "1.3.2"}) {
    SCOPED_TRACE(version);
    const ScopedInstrumentedScenario installed{
        scenarioWithDevices(version, {"parent"})};
    ASSERT_TRUE(installed.loaded());
    mqss::submitter::Client client;
    const auto device =
        client.openDevice(kInstrumentedDriverPath, "parent", {}, {});
    ASSERT_TRUE(device.has_value()) << device.error().message();

    const auto jobs =
        submitQuantumTask(makeTask("OPENQASM 2.0;", 2, "qasm2"), *device);

    ASSERT_TRUE(jobs.has_value()) << jobs.error().detail;
    ASSERT_EQ(jobs->size(), 1U);
    EXPECT_EQ(jobs->front().workflow(), Workflow::Standalone);
  }
}

TEST(RunnersPlacement, DiscoveryFailureDoesNotSubmitOnParent) {
  auto scenario = scenarioWithDevices("1.3.2", {"parent"});
  scenario.children.at(0).firstCallStatus = -8; // Permission denied.
  const ScopedInstrumentedScenario installed{scenario};
  ASSERT_TRUE(installed.loaded());
  mqss::submitter::Client client;
  const auto device =
      client.openDevice(kInstrumentedDriverPath, "parent", {}, {});
  ASSERT_TRUE(device.has_value()) << device.error().message();

  const auto jobs =
      submitQuantumTask(makeTask("OPENQASM 2.0;", 2, "qasm2"), *device);

  ASSERT_FALSE(jobs.has_value());
  EXPECT_EQ(jobs.error().kind, Error::Kind::SubmissionFailed);
  EXPECT_EQ(installed.counters().jobSubmitCount, 0);
}

} // namespace
} // namespace mqss::qrmci::test
