/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file CircuitFixtures.h
/// @brief Access to the sample Quake circuits committed under
///        tests/fixtures/circuits, shared by the unit and integration suites.

#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mqss::qrmci::test {

/// @brief The directory holding the committed sample circuits.
/// @return An absolute path, injected by tests/CMakeLists.txt so that callers
///         do not depend on the working directory ctest happens to use.
inline std::filesystem::path circuitFixtureDir() {
  return std::filesystem::path{QRMCI_TEST_CIRCUIT_FIXTURE_DIR};
}

/// @brief Read one sample circuit into memory.
/// @param name File name within circuitFixtureDir(), e.g. "bell.quake".
/// @return The circuit source, exactly as committed.
/// @throws std::runtime_error if the file is missing or unreadable, so a
///         renamed fixture fails loudly instead of silently compiling "".
inline std::string readCircuitFixture(std::string_view name) {
  const auto path = circuitFixtureDir() / name;
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Missing circuit fixture: " + path.string());
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

} // namespace mqss::qrmci::test
