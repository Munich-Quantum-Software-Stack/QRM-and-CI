/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Config.h"
#include "qrmci/ConfigDefaults.h"
#include "qrmci/Error.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace mqss::qrmci::test {

namespace {
constexpr std::array<const char *, 16> kEnvVars = {
    "QRMCI_CONFIG_FILE",
    "QRMCI_LOG_DIR",
    "QRMCI_AMQP_HOST",
    "QRMCI_AMQP_PORT",
    "QRMCI_AMQP_USER",
    "QRMCI_AMQP_PASSWORD",
    "QRMCI_AMQP_VHOST",
    "QRMCI_QRMCI_QUEUE",
    "QRMCI_SCHEDULER_QUEUE",
    "QRMCI_COMPILER_QUEUE",
    "QRMCI_RESULTS_QUEUE",
    "QRMCI_SUBMITTER_QUEUE",
    "QRMCI_BACKEND_STATUS_QUEUE",
    "QRMCI_SUBMITTER_QDMI_DRIVER_NAME",
    "QRMCI_SUBMITTER_QDMI_DEVICE_NAME",
    "QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN",
};

/// @brief A path that is guaranteed not to exist, for tests exercising the
///        "no config file present" path without touching the real
///        filesystem default.
std::filesystem::path noSuchFile() {
  return std::filesystem::temp_directory_path() /
         "qrmci-test-config-does-not-exist.toml";
}

void clearEnv() {
  for (const char *name : kEnvVars) {
    unsetenv(name);
  }
}

} // namespace

// ===========================================================================
// LoadConfigTest
// Exercises loadConfig()'s environment-variable parsing (the getEnvOr()
// string/int overloads and logPath() joining), which had no coverage at all.
// Every test clears the relevant environment variables in SetUp/TearDown so
// tests remain independent of each other and of whatever the ambient shell
// environment happens to contain. Every test also points at a guaranteed-
// absent file, so behavior is exercised through the (defaults < env) half of
// the precedence chain without a TOML file in play.
// ===========================================================================
class LoadConfigTest : public ::testing::Test {
protected:
  void SetUp() override { clearEnv(); }
  void TearDown() override { clearEnv(); }

  static std::expected<Config, Error> load() {
    return mqss::qrmci::loadConfig(noSuchFile());
  }
};

TEST_F(LoadConfigTest, DefaultsWhenNoEnvOverridesPresent) {
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;

  EXPECT_EQ(config->common.connection.host, mqss::qrmci::defaults::AMPQHost);
  EXPECT_EQ(config->common.connection.port, mqss::qrmci::defaults::AMPQPort);
  EXPECT_EQ(config->common.connection.user, mqss::qrmci::defaults::AMPQUser);
  EXPECT_EQ(config->common.connection.password,
            mqss::qrmci::defaults::AMPQPassword);
  EXPECT_EQ(config->common.connection.vhost, mqss::qrmci::defaults::AMPQVHost);

  EXPECT_EQ(config->common.qrmciQueue, mqss::qrmci::defaults::QRMCIQueue);
  EXPECT_EQ(config->common.schedulerQueue,
            mqss::qrmci::defaults::SchedulerQueue);
  EXPECT_EQ(config->compiler.queue, mqss::qrmci::defaults::CompilerQueue);
  EXPECT_EQ(config->common.resultsQueue, mqss::qrmci::defaults::ResultsQueue);
  EXPECT_EQ(config->submitter.queue, mqss::qrmci::defaults::SubmitterQueue);
  EXPECT_EQ(config->selector.backendStatusQueue,
            mqss::qrmci::defaults::BackendStatusQueue);

  EXPECT_EQ(config->submitter.qdmiDriverName,
            mqss::qrmci::defaults::SubmitterQdmiDriverName);
  EXPECT_EQ(config->submitter.qdmiDeviceName,
            mqss::qrmci::defaults::SubmitterQdmiDeviceName);
  EXPECT_EQ(config->submitter.qdmiClientToken,
            mqss::qrmci::defaults::SubmitterQdmiClientToken);

  // New, file-only tuning fields keep their in-class compiled-in defaults.
  EXPECT_EQ(config->common.stagePollInterval, CommonConfig{}.stagePollInterval);
  EXPECT_EQ(config->selector.backendRegistry.entryTimeToLive,
            BackendRegistryConfig{}.entryTimeToLive);
  EXPECT_EQ(config->selector.backendStatusReceiveTimeout,
            SelectorConfig{}.backendStatusReceiveTimeout);
  EXPECT_EQ(config->selector.taskReceiveTimeout,
            SelectorConfig{}.taskReceiveTimeout);
  EXPECT_EQ(config->compiler.taskReceiveTimeout,
            CompilerConfig{}.taskReceiveTimeout);
  EXPECT_EQ(config->submitter.backendRegistry.entryTimeToLive,
            BackendRegistryConfig{}.entryTimeToLive);
  EXPECT_EQ(config->submitter.backendStatusPublishInterval,
            SubmitterConfig{}.backendStatusPublishInterval);
}

TEST_F(LoadConfigTest, LogDirDefaultsWhenEnvVarUnset) {
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.logDir, mqss::qrmci::defaults::LogDir);
}

TEST_F(LoadConfigTest, LogFilePathsAreJoinedFromLogDir) {
  setenv("QRMCI_LOG_DIR", "/tmp/qrmci-test-logs", 1);
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;

  EXPECT_EQ(config->common.logDir, "/tmp/qrmci-test-logs");
  // Compared against the named defaults (not hardcoded literals) so a future
  // rename in ConfigDefaults.h.in that isn't also wired into Config.cpp
  // fails this test instead of silently drifting.
  EXPECT_EQ(config->common.daemonLog,
            "/tmp/qrmci-test-logs/" +
                std::string(mqss::qrmci::defaults::DaemonLog));
  EXPECT_EQ(config->common.schedulerLog,
            "/tmp/qrmci-test-logs/" +
                std::string(mqss::qrmci::defaults::SchedulerLog));
  EXPECT_EQ(config->compiler.logFile,
            "/tmp/qrmci-test-logs/" +
                std::string(mqss::qrmci::defaults::CompilerLog));
  EXPECT_EQ(config->submitter.logFile,
            "/tmp/qrmci-test-logs/" +
                std::string(mqss::qrmci::defaults::SubmitterLog));
  EXPECT_EQ(config->selector.logFile,
            "/tmp/qrmci-test-logs/" +
                std::string(mqss::qrmci::defaults::SelectorLog));
}

TEST_F(LoadConfigTest, StringEnvVarOverridesDefault) {
  setenv("QRMCI_AMQP_HOST", "rabbitmq.internal", 1);
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.host, "rabbitmq.internal");
}

TEST_F(LoadConfigTest, IntEnvVarOverridesDefault) {
  setenv("QRMCI_AMQP_PORT", "12345", 1);
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.port, 12345);
}

TEST_F(LoadConfigTest, MalformedIntEnvVarFallsBackToDefault) {
  setenv("QRMCI_AMQP_PORT", "not-a-port", 1);
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.port, mqss::qrmci::defaults::AMPQPort);
}

TEST_F(LoadConfigTest, PartiallyNumericIntEnvVarFallsBackToDefault) {
  // from_chars must consume the entire value; a trailing non-digit suffix
  // (e.g. a stray unit or typo) must not be silently truncated to a prefix.
  setenv("QRMCI_AMQP_PORT", "5672extra", 1);
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.port, mqss::qrmci::defaults::AMPQPort);
}

TEST_F(LoadConfigTest, BackendStatusQueueEnvVarOverridesDefault) {
  setenv("QRMCI_BACKEND_STATUS_QUEUE", "custom.backend.status.queue", 1);
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->selector.backendStatusQueue, "custom.backend.status.queue");
}

TEST_F(LoadConfigTest, SubmitterEnvVarsOverrideDefaults) {
  setenv("QRMCI_SUBMITTER_QDMI_DRIVER_NAME", "custom_driver", 1);
  setenv("QRMCI_SUBMITTER_QDMI_DEVICE_NAME", "custom_device", 1);
  setenv("QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN", "custom_token", 1);
  auto config = load();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->submitter.qdmiDriverName, "custom_driver");
  EXPECT_EQ(config->submitter.qdmiDeviceName, "custom_device");
  EXPECT_EQ(config->submitter.qdmiClientToken, "custom_token");
}

TEST_F(LoadConfigTest, RemainingStringEnvVarsOverrideDefaults) {
  // StringEnvVarOverridesDefault, BackendStatusQueueEnvVarOverridesDefault
  // and SubmitterEnvVarsOverrideDefaults above already prove getEnvOr()'s
  // string overload works; this closes the remaining documented env vars
  // that share that exact code path in Config.cpp, each of which had no
  // dedicated override test at all.
  struct Case {
    const char *envVar;
    const char *value;
    std::function<std::string(const Config &)> field;
  };
  const std::vector<Case> cases = {
      {"QRMCI_AMQP_USER", "custom-user",
       [](const Config &c) { return c.common.connection.user; }},
      {"QRMCI_AMQP_PASSWORD", "custom-password",
       [](const Config &c) { return c.common.connection.password; }},
      {"QRMCI_AMQP_VHOST", "/custom-vhost",
       [](const Config &c) { return c.common.connection.vhost; }},
      {"QRMCI_QRMCI_QUEUE", "custom.qrmci.queue",
       [](const Config &c) { return c.common.qrmciQueue; }},
      {"QRMCI_SCHEDULER_QUEUE", "custom.scheduler.queue",
       [](const Config &c) { return c.common.schedulerQueue; }},
      {"QRMCI_COMPILER_QUEUE", "custom.compiler.queue",
       [](const Config &c) { return c.compiler.queue; }},
      {"QRMCI_RESULTS_QUEUE", "custom.results.queue",
       [](const Config &c) { return c.common.resultsQueue; }},
      {"QRMCI_SUBMITTER_QUEUE", "custom.submitter.queue",
       [](const Config &c) { return c.submitter.queue; }},
  };

  for (const auto &testCase : cases) {
    clearEnv();
    setenv(testCase.envVar, testCase.value, 1);
    auto config = load();
    ASSERT_TRUE(config.has_value()) << config.error().detail;
    EXPECT_EQ(testCase.field(*config), testCase.value)
        << "env var: " << testCase.envVar;
  }
}

// ===========================================================================
// LoadConfigFileTest
// Exercises the TOML file half of loadConfig(): a file's values overriding
// defaults, env vars overriding a file's values, a missing file being a
// no-op rather than an error, and a malformed file surfacing as a
// ConfigError. Uses loadConfig(path) directly so each test owns exactly the
// file it writes, rather than fighting over the shared QRMCI_CONFIG_FILE env
// var or a real /etc path.
// ===========================================================================
class LoadConfigFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    clearEnv();
    tempDir =
        std::filesystem::temp_directory_path() /
        ("qrmci-test-config-" +
         std::to_string(
             ::testing::UnitTest::GetInstance()->current_test_info()->line()));
    std::filesystem::create_directories(tempDir);
  }

  void TearDown() override {
    clearEnv();
    std::filesystem::remove_all(tempDir);
  }

  std::filesystem::path writeToml(const std::string &contents) {
    const auto path = tempDir / "qrmci.toml";
    std::ofstream file(path);
    file << contents;
    file.close();
    return path;
  }

  std::filesystem::path tempDir;
};

TEST_F(LoadConfigFileTest, MissingFileIsNotAnError) {
  auto config = mqss::qrmci::loadConfig(noSuchFile());
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.host, mqss::qrmci::defaults::AMPQHost);
}

TEST_F(LoadConfigFileTest, MalformedFileReturnsConfigError) {
  const auto path = writeToml("this is not [ valid toml");
  auto config = mqss::qrmci::loadConfig(path);
  ASSERT_FALSE(config.has_value());
  EXPECT_EQ(config.error().kind, mqss::qrmci::Error::Kind::ConfigError);
}

TEST_F(LoadConfigFileTest, FileValuesOverrideDefaults) {
  const auto path = writeToml(R"(
[common]
qrmciQueue = "file.qrmci.queue"

[common.connection]
host = "file-host"

[selector]
backendStatusQueue = "file.backend.status.queue"

[submitter]
qdmiDriverName = "file_driver"
)");

  auto config = mqss::qrmci::loadConfig(path);
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.host, "file-host");
  EXPECT_EQ(config->common.qrmciQueue, "file.qrmci.queue");
  EXPECT_EQ(config->selector.backendStatusQueue, "file.backend.status.queue");
  EXPECT_EQ(config->submitter.qdmiDriverName, "file_driver");
  // Fields the file left unset still fall back to defaults.
  EXPECT_EQ(config->common.connection.port, mqss::qrmci::defaults::AMPQPort);
}

TEST_F(LoadConfigFileTest, EnvVarOverridesFileValue) {
  const auto path = writeToml(R"(
[common.connection]
host = "file-host"
)");
  setenv("QRMCI_AMQP_HOST", "env-host", 1);

  auto config = mqss::qrmci::loadConfig(path);
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.host, "env-host");
}

TEST_F(LoadConfigFileTest, NewTimingFieldsAreReadFromFile) {
  const auto path = writeToml(R"(
[common]
stagePollInterval = 250

[selector]
backendStatusReceiveTimeout = 111
taskReceiveTimeout = 222

[selector.backendRegistry]
entryTimeToLive = 60

[compiler]
taskReceiveTimeout = 333

[submitter]
backendStatusPublishInterval = 4444

[submitter.backendRegistry]
entryTimeToLive = 45
)");

  auto config = mqss::qrmci::loadConfig(path);
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.stagePollInterval, std::chrono::milliseconds(250));
  EXPECT_EQ(config->selector.backendStatusReceiveTimeout,
            std::chrono::milliseconds(111));
  EXPECT_EQ(config->selector.taskReceiveTimeout,
            std::chrono::milliseconds(222));
  EXPECT_EQ(config->selector.backendRegistry.entryTimeToLive,
            std::chrono::seconds(60));
  EXPECT_EQ(config->compiler.taskReceiveTimeout,
            std::chrono::milliseconds(333));
  EXPECT_EQ(config->submitter.backendStatusPublishInterval,
            std::chrono::milliseconds(4444));
  EXPECT_EQ(config->submitter.backendRegistry.entryTimeToLive,
            std::chrono::seconds(45));
}

// ===========================================================================
// LoadConfigDiscoveryTest
// Exercises the no-argument loadConfig()'s file discovery: QRMCI_CONFIG_FILE
// when set, the relative default path when not.
// ===========================================================================
class LoadConfigDiscoveryTest : public LoadConfigFileTest {};

TEST_F(LoadConfigDiscoveryTest, UsesConfigFileFromEnvVarWhenSet) {
  const auto path = writeToml(R"(
[common.connection]
host = "discovered-via-env"
)");
  setenv("QRMCI_CONFIG_FILE", path.string().c_str(), 1);

  auto config = mqss::qrmci::loadConfig();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.host, "discovered-via-env");
}

TEST_F(LoadConfigDiscoveryTest,
       FallsBackToDefaultsWhenConfigFileEnvVarUnsetAndNoDefaultFileExists) {
  // config/qrmci.toml relative to this test binary's working directory is
  // not created by the build, so this exercises the "no file found" path of
  // the default relative discovery.
  auto config = mqss::qrmci::loadConfig();
  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_EQ(config->common.connection.host, mqss::qrmci::defaults::AMPQHost);
}

} // namespace mqss::qrmci::test
