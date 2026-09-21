/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file RunWithDaemons.h
/// @brief Shared main() body for every tests/integration GTest binary: load
///        config, register a DaemonEnvironment for the given daemon(s), run.

#pragma once

#include "DaemonEnvironment.h"
#include "DaemonProcess.h"
#include "qrmci/Config.h"

#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <utility>
#include <vector>

namespace mqss::qrmci::test {

/// @brief Registers a DaemonEnvironment that owns @p daemons for the
///        lifetime of the run, then runs every TEST_F in this binary.
/// @param extraReadinessGrace Forwarded to DaemonEnvironment -- see its own
///        documentation for why a caller might need this.
/// @return 1 if the shared config could not be loaded; otherwise GTest's own
///         RUN_ALL_TESTS() result.
inline int runWithDaemons(int argc, char **argv,
                          std::vector<DaemonProcess> daemons,
                          std::chrono::milliseconds extraReadinessGrace =
                              std::chrono::milliseconds{0}) {
  ::testing::InitGoogleTest(&argc, argv);

  auto config = mqss::qrmci::loadConfig();
  if (!config) {
    std::cerr << config.error().detail << "\n";
    return 1;
  }

  ::testing::AddGlobalTestEnvironment(new DaemonEnvironment(
      std::move(daemons), config->common.connection, extraReadinessGrace));

  return RUN_ALL_TESTS();
}

} // namespace mqss::qrmci::test
