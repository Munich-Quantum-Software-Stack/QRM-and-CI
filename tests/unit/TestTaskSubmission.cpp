/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "RunnersTestHelpers.h"
#include "TaskSubmission.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

namespace mqss::qrmci::test {

TEST(TaskSubmissionTest, RejectsInvalidShapeAndUnknownFormat) {
  for (const int scenario : {0, 1, 2, 3}) {
    SCOPED_TRACE(scenario);
    auto task = makeTask("OPENQASM 2.0;", 2, "qasm2");
    if (scenario == 0) {
      task.clear_circuit_files();
    }
    if (scenario == 1) {
      task.set_n_shots(0);
    }
    if (scenario == 2) {
      task.set_n_shots(-1);
    }
    if (scenario == 3) {
      task.set_circuit_file_type("unknown");
    }
    auto result = detail::validateSubmission(task);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Error::Kind::SubmissionFailed);
  }
}

TEST(TaskSubmissionTest, PreservesBinaryPayloadAndShots) {
  auto task = makeTask(std::string("a\0b", 3), 2, "qasm2");
  task.set_n_shots(17);
  auto format = detail::validateSubmission(task);
  ASSERT_TRUE(format);
  auto request = detail::makeJobRequest(task, 0, *format);
  EXPECT_EQ(request.format, mqss::CircuitFormat::CIRCUIT_FORMAT_QASM2);
  EXPECT_EQ(request.numShots, 17U);
  EXPECT_EQ(
      request.payload,
      (std::vector<std::byte>{std::byte{'a'}, std::byte{0}, std::byte{'b'}}));
}

} // namespace mqss::qrmci::test
