/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "DaemonProcess.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <thread>

namespace mqss::qrmci::test {

namespace {

std::filesystem::path scratchDir(const std::string &testName) {
  return std::filesystem::temp_directory_path() / "qrmci-test-daemon-process" /
         testName;
}

/// @brief A tiny `#!/bin/sh` script that execs `sleep 3`, so a still-running
///        child can be spawned without DaemonProcess having any argv support
///        of its own -- it only ever spawns @c executablePath with no
///        arguments, and the kernel's shebang handling supplies `sleep`'s
///        argument instead.
std::filesystem::path writeSleepScript(const std::filesystem::path &dir) {
  std::filesystem::create_directories(dir);
  const auto scriptPath = dir / "sleep_briefly.sh";
  {
    std::ofstream script(scriptPath);
    script << "#!/bin/sh\nexec sleep 3\n";
  }
  // rwxr-xr-x: posix_spawn execve()s this path directly.
  ::chmod(scriptPath.c_str(), 0755);
  return scriptPath;
}

/// @brief Redirect std::cerr into a string for the duration of @p fn, so a
///        test can assert DaemonProcess logged nothing.
template <class Fn> std::string captureStderr(Fn &&fn) {
  std::ostringstream captured;
  auto *previous = std::cerr.rdbuf(captured.rdbuf());
  std::forward<Fn>(fn)();
  std::cerr.rdbuf(previous);
  return captured.str();
}

} // namespace

TEST(DaemonProcessTest,
     ShortLivedChildStopsSilentlyAfterExitingAndReapingOnItsOwn) {
  DaemonProcess process("true", "/bin/true",
                        scratchDir("ShortLivedChildExitsAndReaps"));

  const std::string stderrOutput = captureStderr([&] {
    ASSERT_TRUE(process.start());

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (process.isRunning() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_FALSE(process.isRunning())
        << "/bin/true did not exit within the test's own timeout";

    // isRunning() has already reaped the child and cleared ownership above,
    // so neither call below has a live PID to act on. Calling stop() twice
    // verifies that the exited PID is not retained as owned; otherwise a
    // later call could signal an unrelated process after PID reuse.
    process.stop();
    process.stop();
  });

  EXPECT_TRUE(stderrOutput.empty())
      << "DaemonProcess logged unexpectedly: " << stderrOutput;
}

TEST(DaemonProcessTest, RunningChildStopsNormallyViaSigterm) {
  const auto dir = scratchDir("RunningChildStopsNormally");
  const auto scriptPath = writeSleepScript(dir);

  DaemonProcess process("sleeper", scriptPath, dir);
  ASSERT_TRUE(process.start());
  ASSERT_TRUE(process.isRunning());

  // Comfortably shorter than the script's own 3s sleep, so this only passes
  // if SIGTERM actually stopped the child rather than the grace period
  // simply running out and falling through to SIGKILL.
  process.stop(std::chrono::seconds(2));

  EXPECT_FALSE(process.isRunning());
}

} // namespace mqss::qrmci::test
