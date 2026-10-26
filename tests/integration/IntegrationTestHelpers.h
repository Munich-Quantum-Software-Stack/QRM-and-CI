/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file IntegrationTestHelpers.h
/// @brief Shared fixtures for the tests/integration client programs, which
///        exercise a live QRM&CI deployment (apps/standalone or
///        apps/distributed) over a real RabbitMQ/QDMI setup.

#pragma once

#include "CircuitFixtures.h"

#include <chrono>
#include <cstdint>
#include <random>
#include <string>

namespace mqss::qrmci::test {

/// @brief Budget for a client waiting on exactly one result. Comfortably
///        under the 120s ctest TIMEOUT set on every tests/integration
///        target (tests/integration/CMakeLists.txt), which is the hard
///        backstop if a broker or daemon problem makes this too short.
inline constexpr std::chrono::seconds kResultTimeout{30};

/// @brief Per-receive budget for a client that polls the same queue
///        multiple times in one run (TestSubmitTaskConcurrent,
///        TestScheduleTask): sized so the worst case, every poll timing out,
///        still finishes inside the 120s ctest TIMEOUT.
inline constexpr std::chrono::seconds kPolledResultTimeout{10};

/// @brief A task id unlikely to collide with a result an earlier run left
///        on a durable queue. RabbitMQ queues here are durable, so a fixed
///        id lets a stale leftover satisfy this run's assertions without
///        the daemon having done anything.
/// @return A fresh random id in [1, INT32_MAX] on every call.
inline std::int32_t uniqueTaskId() {
  static std::random_device entropy;
  static std::mt19937 engine(entropy());
  static std::uniform_int_distribution<std::int32_t> dist(1, 2147483647);
  return dist(engine);
}

/// @brief A compilable/executable 2-qubit Quake circuit, read from the
///        sample circuits shared with the unit suites.
/// @return The Quake circuit source.
inline const std::string &sampleQuakeCircuit() {
  static const std::string circuit = readCircuitFixture("toggle-chain.quake");
  return circuit;
}

} // namespace mqss::qrmci::test
