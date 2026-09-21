/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Error.h"

#include <array>
#include <gtest/gtest.h>
#include <string>

namespace mqss::qrmci::test {

using Kind = mqss::qrmci::Error::Kind;

/// @brief Every enumerator, so the tests below cannot silently skip a kind
///        added later.
constexpr std::array<Kind, 10> AllKinds = {
    Kind::NoBackendAvailable, Kind::UnsupportedFormat, Kind::CompilationFailed,
    Kind::SubmissionFailed,   Kind::DeviceError,       Kind::MessagingFailed,
    Kind::Internal,           Kind::ConfigError,       Kind::DriverUnavailable,
    Kind::ShutdownRequested};

// ===========================================================================
// toString
// ===========================================================================
TEST(ErrorToStringTest, NamesEveryKindDistinctly) {
  for (const auto kind : AllKinds) {
    EXPECT_FALSE(toString(kind).empty());
    EXPECT_NE(toString(kind), "Unknown");
    for (const auto other : AllKinds) {
      if (kind != other) {
        EXPECT_NE(toString(kind), toString(other));
      }
    }
  }
}

TEST(ErrorToStringTest, UsesTheEnumeratorName) {
  EXPECT_EQ(toString(Kind::NoBackendAvailable), "NoBackendAvailable");
  EXPECT_EQ(toString(Kind::UnsupportedFormat), "UnsupportedFormat");
  EXPECT_EQ(toString(Kind::CompilationFailed), "CompilationFailed");
  EXPECT_EQ(toString(Kind::SubmissionFailed), "SubmissionFailed");
  EXPECT_EQ(toString(Kind::DeviceError), "DeviceError");
  EXPECT_EQ(toString(Kind::MessagingFailed), "MessagingFailed");
  EXPECT_EQ(toString(Kind::Internal), "Internal");
  EXPECT_EQ(toString(Kind::ConfigError), "ConfigError");
}

TEST(ErrorToStringTest, DriverUnavailableRoundTrips) {
  EXPECT_EQ(toString(Kind::DriverUnavailable), "DriverUnavailable");
}

TEST(ErrorToStringTest, ShutdownRequestedRoundTrips) {
  EXPECT_EQ(toString(Kind::ShutdownRequested), "ShutdownRequested");
}

// ===========================================================================
// The struct itself
// ===========================================================================
TEST(ErrorTest, CarriesTheDetailUnchanged) {
  // The detail is what reaches the log line and the CANCELLED result, so it
  // must survive being carried around verbatim.
  const std::string detail = "Compilation failed for circuit file 1.";
  const Error error{Kind::CompilationFailed, detail};
  EXPECT_EQ(error.detail, detail);
  EXPECT_EQ(error.kind, Kind::CompilationFailed);
}

} // namespace mqss::qrmci::test
