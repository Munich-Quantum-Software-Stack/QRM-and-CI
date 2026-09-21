/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Logger.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>

namespace mqss::qrmci::test {

// ===========================================================================
// MakeLoggerTest
// ===========================================================================
TEST(MakeLoggerTest, LoggerNameMatches) {
  auto logger = makeLogger("test-logger");
  EXPECT_EQ(logger->name(), "test-logger");
}

TEST(MakeLoggerTest, DefaultLevelIsInfo) {
  auto logger = makeLogger("test-logger");
  EXPECT_EQ(logger->level(), spdlog::level::info);
}

TEST(MakeLoggerTest, NoLogFileMeansOneSink) {
  auto logger = makeLogger("test-logger");
  EXPECT_EQ(logger->sinks().size(), 1U);
}

TEST(MakeLoggerTest, LogFileAddsASecondSink) {
  const auto path =
      std::filesystem::temp_directory_path() / "qrmci-test-logger-sink.log";
  std::filesystem::remove(path);
  auto logger = makeLogger("test-logger", path.string());
  EXPECT_EQ(logger->sinks().size(), 2U);
  std::filesystem::remove(path);
}

TEST(MakeLoggerTest, LogFileIsCreatedOnDisk) {
  const auto path =
      std::filesystem::temp_directory_path() / "qrmci-test-logger-file.log";
  std::filesystem::remove(path);
  auto logger = makeLogger("test-logger", path.string());
  logger->info("hello");
  logger->flush();
  EXPECT_TRUE(std::filesystem::exists(path));

  // Verify the file contains the logged message and logger name
  std::ifstream logfile(path);
  std::string contents((std::istreambuf_iterator<char>(logfile)),
                       std::istreambuf_iterator<char>());
  EXPECT_FALSE(contents.empty());
  EXPECT_NE(contents.find("hello"), std::string::npos);
  EXPECT_NE(contents.find("test-logger"), std::string::npos);

  std::filesystem::remove(path);
}

TEST(MakeLoggerTest, UnwritableLogFileFallsBackToConsoleOnly) {
  auto logger = makeLogger("test-logger", "/dev/null/logs/daemon.log");
  ASSERT_NE(logger, nullptr);
  EXPECT_EQ(logger->sinks().size(), 1U);
}

TEST(MakeLoggerTest, ExistingLogContentSurvivesANewLogger) {
  const auto path =
      std::filesystem::temp_directory_path() / "qrmci-append-test.log";
  std::filesystem::remove(path);
  {
    std::ofstream seed(path);
    seed << "PRIOR RUN\n";
  }
  auto logger = makeLogger("test-logger", path.string());
  logger->info("current run");
  logger->flush();

  std::ifstream f(path);
  const std::string contents((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
  EXPECT_NE(contents.find("PRIOR RUN"), std::string::npos);
  EXPECT_NE(contents.find("current run"), std::string::npos);

  std::filesystem::remove(path);
}

} // namespace mqss::qrmci::test
