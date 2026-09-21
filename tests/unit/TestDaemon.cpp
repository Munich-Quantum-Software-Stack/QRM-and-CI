/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "RunnersTestHelpers.h"
#include "mqss/Protocol.hpp"
#include "qrmci/BackendRegistry.h"
#include "qrmci/CommunicationHandler.h"
#include "qrmci/Config.h"
#include "qrmci/Daemon.h"
#include "qrmci/DaemonMessaging.h"
#include "qrmci/Error.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

namespace mqss::qrmci::test {

namespace {

// QRMCI_TEST_QDMI_DRIVER_PATH is set in tests/unit/CMakeLists.txt to the built
// example driver's own file; QDMI_CONF and LD_LIBRARY_PATH are injected by the
// same file's set_tests_properties(), which is why this suite must be run
// through ctest rather than by invoking the binary directly.
constexpr const char *kDriverPath = QRMCI_TEST_QDMI_DRIVER_PATH;
constexpr const char *kDeviceName = "C++ Device with 5 qubits";

/// @brief A valid Quake circuit, the same shape TestRunnersTaskCompilation
///        compiles: the compiler only ever accepts quake/catalyst input.
constexpr const char *kValidQuakeCircuit = R"(
      module {
      func.func @hadamard_circuit() {
        %q0 = quake.alloca !quake.ref
        quake.h %q0 : (!quake.ref) -> ()
        %b0 = quake.mz %q0 : (!quake.ref) -> !quake.measure
        return
      }
    }
  )";

/// @brief A connection to a port no AMQP broker listens on, so every send and
///        receive through it fails the way a broker outage would.
RabbitMqConnectionConfig unreachableBroker() {
  return RabbitMqConnectionConfig{
      .host = "127.0.0.1",
      .port = 5673,
      .user = "guest",
      .password = "guest",
      .vhost = "/",
  };
}

mqss::QuantumTask makeCompilableTask(const std::string &backendName) {
  auto task = makeTask(kValidQuakeCircuit, 2, "quake");
  task.set_task_id(7);
  task.set_result_destination("results-queue");
  task.set_scheduled_qpu(backendName);
  return task;
}

BackendRegistry registryWith(const BackendWrapper &backend) {
  BackendRegistry backends;
  backends.insertOrRefresh(backend);
  return backends;
}

/// @brief A backend the compiler can actually target: QIRBASESTRING is the
///        one supported circuit format with a compiler result format.
BackendWrapper compilableBackend(const std::string &name = "alpha",
                                 std::uint32_t numQubits = 5) {
  return makeBackend(name, numQubits, mqss::BackendStatus::BACKEND_STATUS_IDLE,
                     {mqss::CircuitFormat::CIRCUIT_FORMAT_QIRBASESTRING});
}

} // namespace

// ===========================================================================
// compileAndSchedule
// ===========================================================================
class CompileAndScheduleTest : public ::testing::Test {
protected:
  mqss::mqssci::MQSSCompiler compiler;
  TaskScheduler scheduler{mqss::scheduler::SchedulingPolicy::PriorityBased};
};

TEST_F(CompileAndScheduleTest, CompilesAndSchedulesTaskForRegisteredBackend) {
  auto backends = registryWith(compilableBackend());
  auto task = makeCompilableTask("alpha");
  const std::string originalCircuit = task.circuit_files(0);

  auto scheduled = compileAndSchedule(task, backends, compiler, scheduler);

  ASSERT_TRUE(scheduled.has_value()) << scheduled.error().detail;
  EXPECT_EQ(scheduler.getTaskCount(), 1);
  EXPECT_NE(task.circuit_files(0), originalCircuit)
      << "the scheduled task should carry the compiled circuit";
}

TEST_F(CompileAndScheduleTest, UnregisteredScheduledQpuIsNoBackendAvailable) {
  auto backends = registryWith(compilableBackend());
  auto task = makeCompilableTask("no-such-backend");

  auto scheduled = compileAndSchedule(task, backends, compiler, scheduler);

  ASSERT_FALSE(scheduled.has_value());
  EXPECT_EQ(scheduled.error().kind, Error::Kind::NoBackendAvailable);
  EXPECT_NE(scheduled.error().detail.find("no-such-backend"),
            std::string::npos);
  EXPECT_EQ(scheduler.getTaskCount(), 0);
}

TEST_F(CompileAndScheduleTest, CompilationFailureLeavesNothingScheduled) {
  auto backends = registryWith(compilableBackend());
  auto task = makeCompilableTask("alpha");
  task.set_circuit_files(0, "THIS IS NOT VALID QUAKE MLIR SYNTAX;");

  auto scheduled = compileAndSchedule(task, backends, compiler, scheduler);

  ASSERT_FALSE(scheduled.has_value());
  EXPECT_EQ(scheduled.error().kind, Error::Kind::CompilationFailed);
  EXPECT_EQ(scheduler.getTaskCount(), 0);
}

// ===========================================================================
// selectCompileAndSchedule
// ===========================================================================
using SelectCompileAndScheduleTest = CompileAndScheduleTest;

TEST_F(SelectCompileAndScheduleTest, AssignsTheChosenBackendToTheTask) {
  auto backends = registryWith(compilableBackend("alpha"));
  // Deliberately left unassigned: selection, not the caller, fills this in.
  auto task = makeCompilableTask("");

  auto prepared = selectCompileAndSchedule(task, backends, compiler, scheduler);

  ASSERT_TRUE(prepared.has_value()) << prepared.error().detail;
  EXPECT_EQ(task.scheduled_qpu(), "alpha");
  EXPECT_EQ(scheduler.getTaskCount(), 1);
}

TEST_F(SelectCompileAndScheduleTest,
       SmallestSufficientPolicyChoosesTheSmallestCompatibleBackend) {
  // Proves the policy parameter actually reaches chooseBackend() through the
  // whole composition, not just that BackendSelectionPolicy::SmallestSufficient
  // works in isolation (already covered by TestRunnersBackendSelection).
  BackendRegistry backends;
  backends.insertOrRefresh(compilableBackend("small", 5));
  backends.insertOrRefresh(compilableBackend("large", 10));
  // Deliberately left unassigned: selection, not the caller, fills this in.
  auto task = makeCompilableTask("");

  auto prepared = selectCompileAndSchedule(
      task, backends, compiler, scheduler,
      mqss::qrmci::BackendSelectionPolicy::SmallestSufficient);

  ASSERT_TRUE(prepared.has_value()) << prepared.error().detail;
  EXPECT_EQ(task.scheduled_qpu(), "small");
  EXPECT_EQ(scheduler.getTaskCount(), 1);
}

TEST_F(SelectCompileAndScheduleTest, EmptyRegistryIsNoBackendAvailable) {
  BackendRegistry backends;
  auto task = makeCompilableTask("");

  auto prepared = selectCompileAndSchedule(task, backends, compiler, scheduler);

  ASSERT_FALSE(prepared.has_value());
  EXPECT_EQ(prepared.error().kind, Error::Kind::NoBackendAvailable);
  EXPECT_EQ(scheduler.getTaskCount(), 0);
}

// ===========================================================================
// openConfiguredDevice / registerOwnDevice / refreshOwnDeviceStatus
// ===========================================================================
class ConfiguredDeviceTest : public ::testing::Test {
protected:
  static SubmitterConfig config() {
    SubmitterConfig submitter;
    submitter.qdmiDriver = kDriverPath;
    submitter.qdmiDeviceName = kDeviceName;
    submitter.qdmiClientToken = "example_token";
    return submitter;
  }
};

TEST_F(ConfiguredDeviceTest, OpensTheConfiguredDevice) {
  auto device = openConfiguredDevice(config());
  ASSERT_TRUE(device.has_value()) << device.error().detail;
  EXPECT_EQ(device->name(), kDeviceName);
}

TEST_F(ConfiguredDeviceTest, MissingDriverIsDriverUnavailable) {
  auto submitter = config();
  submitter.qdmiDriver = "/nonexistent/libqdmi_driver.so";

  auto device = openConfiguredDevice(submitter);

  ASSERT_FALSE(device.has_value());
  EXPECT_EQ(device.error().kind, Error::Kind::DriverUnavailable);
}

TEST_F(ConfiguredDeviceTest, DeviceIdOverridesTheRegisteredBackendName) {
  auto submitter = config();
  submitter.qdmiDeviceId = "renamed-device";
  auto device = openConfiguredDevice(submitter);
  ASSERT_TRUE(device.has_value()) << device.error().detail;

  BackendRegistry backends;
  auto registered = registerOwnDevice(*device, backends);

  ASSERT_TRUE(registered.has_value()) << registered.error().detail;
  EXPECT_TRUE(backends.contains("renamed-device"));
}

TEST_F(ConfiguredDeviceTest, RegisterOwnDeviceCarriesTheDispatchQueue) {
  auto device = openConfiguredDevice(config());
  ASSERT_TRUE(device.has_value()) << device.error().detail;

  BackendRegistry backends;
  auto registered = registerOwnDevice(*device, backends, "compiler.worker-a");
  ASSERT_TRUE(registered.has_value()) << registered.error().detail;

  const auto *found = backends.find(std::string(device->deviceId()));
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getQueueName(), "compiler.worker-a");
}

TEST_F(ConfiguredDeviceTest, RegisterOwnDeviceDefaultsToAnEmptyDispatchQueue) {
  // A process that never publishes its own status, such as the standalone
  // daemon, has no dispatch queue of its own to advertise.
  auto device = openConfiguredDevice(config());
  ASSERT_TRUE(device.has_value()) << device.error().detail;

  BackendRegistry backends;
  auto registered = registerOwnDevice(*device, backends);
  ASSERT_TRUE(registered.has_value()) << registered.error().detail;

  const auto *found = backends.find(std::string(device->deviceId()));
  ASSERT_NE(found, nullptr);
  EXPECT_TRUE(found->getQueueName().empty());
}

TEST_F(ConfiguredDeviceTest, RegisterOwnDeviceRegistersTheOpenedDevice) {
  auto device = openConfiguredDevice(config());
  ASSERT_TRUE(device.has_value()) << device.error().detail;

  BackendRegistry backends;
  auto registered = registerOwnDevice(*device, backends);

  ASSERT_TRUE(registered.has_value()) << registered.error().detail;
  EXPECT_EQ(backends.size(), 1);
  // Listing a registry with an entry in it must not fault.
  EXPECT_NO_THROW(logRegisteredBackends(backends));
}

TEST_F(ConfiguredDeviceTest,
       LogRegisteredBackendsNamesTheStatusInsteadOfANumber) {
  auto device = openConfiguredDevice(config());
  ASSERT_TRUE(device.has_value()) << device.error().detail;

  BackendRegistry backends;
  auto registered = registerOwnDevice(*device, backends);
  ASSERT_TRUE(registered.has_value()) << registered.error().detail;

  std::ostringstream captured;
  auto captureSink = std::make_shared<spdlog::sinks::ostream_sink_mt>(captured);
  auto captureLogger = std::make_shared<spdlog::logger>("capture", captureSink);
  captureLogger->set_level(spdlog::level::info);
  auto previousLogger = spdlog::default_logger();
  spdlog::set_default_logger(captureLogger);

  logRegisteredBackends(backends);

  spdlog::set_default_logger(previousLogger);

  // Every BackendStatus enumerator is named BACKEND_STATUS_*, whichever
  // status the example device happens to report, so the prefix alone proves
  // the log carries a stable name rather than the raw integer it used to.
  EXPECT_NE(captured.str().find("BACKEND_STATUS_"), std::string::npos)
      << captured.str();
}

TEST_F(ConfiguredDeviceTest, refreshOwnDeviceStatusPublishesOncePerInterval) {
  auto device = openConfiguredDevice(config());
  ASSERT_TRUE(device.has_value()) << device.error().detail;

  BackendRegistry backends(std::chrono::hours(1));
  // An hour-long publish interval, so only the very first claim succeeds.
  PublicationThrottle throttle(std::chrono::hours(1));

  auto firstStatus = refreshOwnDeviceStatus(*device, backends, throttle);
  ASSERT_TRUE(firstStatus.has_value());
  EXPECT_EQ(firstStatus->name(), std::string(device->deviceId()));
  EXPECT_EQ(backends.size(), 1);

  EXPECT_FALSE(refreshOwnDeviceStatus(*device, backends, throttle).has_value())
      << "a second refresh within the publish interval must claim no slot";
}

// ===========================================================================
// receiveNext / drainQueue  (broker unreachable: the failure path)
// ===========================================================================
class QueueDrainTest : public ::testing::Test {
protected:
  CommunicationHandler comms{unreachableBroker()};
  std::atomic<bool> terminationFlag{false};
};

TEST_F(QueueDrainTest, ReceiveNextReportsNoMessageWhenTheBrokerIsUnreachable) {
  bool handled = false;
  const bool received = receiveNext<mqss::QuantumTask>(
      comms, "some-queue", std::chrono::milliseconds(0), terminationFlag,
      [&handled](const mqss::QuantumTask &) { handled = true; });

  EXPECT_FALSE(received);
  EXPECT_FALSE(handled);
}

TEST_F(QueueDrainTest, DrainQueueHandlesNothingWhenTheBrokerIsUnreachable) {
  int handledCount = 0;
  const bool handledAny = drainQueue<mqss::QuantumTask>(
      comms, "some-queue", terminationFlag,
      [&handledCount](const mqss::QuantumTask &) { ++handledCount; });

  EXPECT_FALSE(handledAny);
  EXPECT_EQ(handledCount, 0);
}

// ===========================================================================
// receiveNext / drainQueue  (live broker: the termination-flag passthrough)
// The unreachable-broker fixture above cannot tell a flag-driven early return
// apart from a broker-failure early return -- both report false. Proving the
// flag genuinely reaches CommunicationHandler::receive() (rather than being
// accepted and ignored) needs a message actually sitting on the queue that a
// preset flag must leave unconsumed. Skipped outside an integration
// environment with a broker at QRMCI_TEST_AMQP_HOST, the same opt-in
// TestCommunicationHandler.cpp's live-broker suite uses.
// ===========================================================================
class QueueDrainLiveBrokerTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (std::getenv("QRMCI_TEST_AMQP_HOST") == nullptr) {
      GTEST_SKIP()
          << "QRMCI_TEST_AMQP_HOST not set; skipping live broker tests";
    }
  }

  static RabbitMqConnectionConfig config() {
    const char *host = std::getenv("QRMCI_TEST_AMQP_HOST");
    return RabbitMqConnectionConfig{
        .host = host != nullptr ? host : "127.0.0.1",
        .port = 5672,
        .user = "guest",
        .password = "guest",
        .vhost = "/",
    };
  }
  CommunicationHandler comms{config()};
};

TEST_F(QueueDrainLiveBrokerTest, ReceiveNextHonoursAPreSetTerminationFlag) {
  const std::string queue = "test.daemonmessaging.receivenext.flag";
  mqss::QuantumTask task;
  task.set_task_id(7);
  ASSERT_TRUE(comms.send(task, queue));

  const std::atomic<bool> terminationFlag{true};
  bool handled = false;
  const bool received = receiveNext<mqss::QuantumTask>(
      comms, queue, std::chrono::milliseconds(500), terminationFlag,
      [&handled](const mqss::QuantumTask &) { handled = true; });

  EXPECT_FALSE(received);
  EXPECT_FALSE(handled);
}

TEST_F(QueueDrainLiveBrokerTest, DrainQueueHonoursAPreSetTerminationFlag) {
  const std::string queue = "test.daemonmessaging.drainqueue.flag";
  mqss::QuantumTask task;
  task.set_task_id(8);
  ASSERT_TRUE(comms.send(task, queue));

  const std::atomic<bool> terminationFlag{true};
  int handledCount = 0;
  const bool handledAny = drainQueue<mqss::QuantumTask>(
      comms, queue, terminationFlag,
      [&handledCount](const mqss::QuantumTask &) { ++handledCount; });

  EXPECT_FALSE(handledAny);
  EXPECT_EQ(handledCount, 0);
}

// ===========================================================================
// send / sendFailure / sendResult
// ===========================================================================
TEST(DaemonSendTest, SendsAreBestEffortAgainstAnUnreachableBroker) {
  CommunicationHandler comms{unreachableBroker()};
  const auto task = makeCompilableTask("alpha");

  // A daemon work loop must survive a broker outage: every one of these
  // fails at the transport and must still return normally.
  EXPECT_NO_THROW(send(comms, task, "some-queue"));
  EXPECT_NO_THROW(sendFailure(
      comms, task, Error{Error::Kind::DeviceError, "device fell over"},
      "failed."));
  EXPECT_NO_THROW(sendResult(comms, task, mqss::QuantumResult{}));
}

// ===========================================================================
// initializeDaemon
// ===========================================================================
TEST(InitializeDaemonTest, MalformedConfigFileIsReportedAsAConfigError) {
  const auto path = std::filesystem::temp_directory_path() /
                    "qrmci-test-daemon-malformed.toml";
  {
    std::ofstream file(path);
    file << "this is not = = valid toml\n";
  }
  setenv("QRMCI_CONFIG_FILE", path.string().c_str(), 1);

  auto config = initializeDaemon();

  unsetenv("QRMCI_CONFIG_FILE");
  std::filesystem::remove(path);

  ASSERT_FALSE(config.has_value());
  EXPECT_EQ(config.error().kind, Error::Kind::ConfigError);
}

TEST(InitializeDaemonTest, MissingConfigFileStillYieldsADefaultedConfig) {
  setenv("QRMCI_CONFIG_FILE", "/nonexistent/qrmci.toml", 1);

  auto config = initializeDaemon();

  unsetenv("QRMCI_CONFIG_FILE");

  ASSERT_TRUE(config.has_value()) << config.error().detail;
  EXPECT_FALSE(config->common.qrmciQueue.empty());
}

// ===========================================================================
// installTerminationHandlers / terminationRequested
// ===========================================================================
class TerminationHandlersTest : public ::testing::Test {
protected:
  void SetUp() override {
    terminationRequested.store(false, std::memory_order_relaxed);
  }
};

TEST_F(TerminationHandlersTest, InstallTerminationHandlersSetsFlagOnSigint) {
  installTerminationHandlers();
  ASSERT_EQ(std::raise(SIGINT), 0);
  EXPECT_TRUE(terminationRequested.load(std::memory_order_relaxed));
}

TEST_F(TerminationHandlersTest, InstallTerminationHandlersSetsFlagOnSigterm) {
  installTerminationHandlers();
  ASSERT_EQ(std::raise(SIGTERM), 0);
  EXPECT_TRUE(terminationRequested.load(std::memory_order_relaxed));
}

} // namespace mqss::qrmci::test
