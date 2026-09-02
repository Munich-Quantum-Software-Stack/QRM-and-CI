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
constexpr std::array<Kind, 8> AllKinds = {
    Kind::NoBackendAvailable, Kind::UnsupportedFormat, Kind::CompilationFailed,
    Kind::SubmissionFailed,   Kind::DeviceError,       Kind::MessagingFailed,
    Kind::Internal,           Kind::ConfigError};

// ===========================================================================
// isRetryable
// ===========================================================================
TEST(ErrorIsRetryableTest, TransientKindsAreRetryable) {
  // A backend that is not up yet, a device that refused or misbehaved, and a
  // broker that could not be reached are all worth trying again unchanged.
  EXPECT_TRUE((Error{Kind::NoBackendAvailable, "no backend"}).isRetryable());
  EXPECT_TRUE((Error{Kind::SubmissionFailed, "refused"}).isRetryable());
  EXPECT_TRUE((Error{Kind::DeviceError, "no counts"}).isRetryable());
  EXPECT_TRUE((Error{Kind::MessagingFailed, "broker down"}).isRetryable());
}

TEST(ErrorIsRetryableTest, PermanentKindsAreNotRetryable) {
  // These are the caller's input: the same task will fail the same way for as
  // long as it is resubmitted, so it must be rejected rather than requeued.
  EXPECT_FALSE((Error{Kind::UnsupportedFormat, "bad format"}).isRetryable());
  EXPECT_FALSE((Error{Kind::CompilationFailed, "bad circuit"}).isRetryable());
}

TEST(ErrorIsRetryableTest, InternalIsNotRetryable) {
  // A violated invariant re-runs the same broken path on a retry; it is for a
  // human to look at, not for the pipeline to work around.
  EXPECT_FALSE((Error{Kind::Internal, "invariant violated"}).isRetryable());
}

TEST(ErrorIsRetryableTest, EveryKindIsClassified) {
  // isRetryable() must answer for every enumerator rather than falling
  // through its switch -- the point of the type is that no kind is
  // unclassified.
  for (const auto kind : AllKinds) {
    const Error error{kind, "detail"};
    const bool transient =
        kind == Kind::NoBackendAvailable || kind == Kind::SubmissionFailed ||
        kind == Kind::DeviceError || kind == Kind::MessagingFailed;
    EXPECT_EQ(error.isRetryable(), transient)
        << "kind: " << toString(kind) << " is classified wrongly";
  }
}

TEST(ErrorIsRetryableTest, DetailDoesNotAffectTheAnswer) {
  // The whole point of the change: the decision comes from the kind, never
  // from the wording.
  EXPECT_EQ((Error{Kind::CompilationFailed, ""}).isRetryable(),
            (Error{Kind::CompilationFailed, "circuit file 3 is empty"})
                .isRetryable());
}

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
