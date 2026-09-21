/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/Daemon.h"

#include "mqss/Protocol.hpp"
#include "qrmci/BackendRegistry.h"
#include "qrmci/BackendWrapper.h"
#include "qrmci/Config.h"
#include "qrmci/ConstantsMapping.h"
#include "qrmci/Error.h"
#include "qrmci/Logger.h"
#include "qrmci/PublicationThrottle.h"
#include "qrmci/Runners.h"

#include <MQSSCIInterfaces/MQSSCompiler.h>
#include <expected>
#include <filesystem>
#include <mqss/submitter/Client.h>
#include <mqss/submitter/Device.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>

namespace mqss::qrmci {

namespace {

/// @brief Read an optional string configuration field: empty means "unset",
///        not "the empty string", which is what the QDMI session and device
///        options distinguish.
std::optional<std::string> optionalSetting(const std::string &value) {
  return value.empty() ? std::nullopt : std::optional{value};
}

} // namespace

std::expected<Config, Error> initializeDaemon() {
  installTerminationHandlers();

  auto config = loadConfig();
  if (!config) {
    return config;
  }
  spdlog::set_default_logger(
      makeLogger(config->common.daemonLogger, config->common.daemonLog));
  return config;
}

std::expected<mqss::submitter::Device, Error>
openConfiguredDevice(const SubmitterConfig &config) {
  // Both option structs are default-initialized and then assigned field by
  // field: every other member stays at its own "unset" default without this
  // file having to name (and keep naming) them.
  mqss::submitter::SessionConfig session;
  session.token = optionalSetting(config.qdmiClientToken);
  mqss::submitter::DeviceConfig deviceOptions;
  deviceOptions.deviceId = optionalSetting(config.qdmiDeviceId);

  mqss::submitter::Client client;
  auto opened =
      client.openDevice(std::filesystem::path{config.qdmiDriver},
                        config.qdmiDeviceName, session, deviceOptions);
  if (!opened) {
    return std::unexpected(
        toQrmciError(Error::Kind::DriverUnavailable, opened.error()));
  }
  // `client` need not outlive this: Device owns a shared_ptr to the driver's
  // control block internally.
  return std::move(*opened);
}

std::expected<void, Error>
registerOwnDevice(const mqss::submitter::Device &device,
                  BackendRegistry &backends, std::string_view dispatchQueue) {
  auto backend = BackendWrapper::fromSubmitter(device, dispatchQueue);
  if (!backend) {
    return std::unexpected(std::move(backend.error()));
  }
  backends.insertOrRefresh(std::move(*backend));
  return {};
}

std::optional<mqss::Backend>
refreshOwnDeviceStatus(const mqss::submitter::Device &device,
                       BackendRegistry &backends, PublicationThrottle &throttle,
                       std::string_view dispatchQueue) {
  if (!throttle.claim()) {
    return std::nullopt;
  }

  auto refreshed = BackendWrapper::fromSubmitter(device, dispatchQueue);
  if (!refreshed) {
    spdlog::warn("Could not refresh the backend status: [{}] {}",
                 toString(refreshed.error().kind), refreshed.error().detail);
    return std::nullopt;
  }

  auto status = refreshed->toBackend();
  backends.insertOrRefresh(std::move(*refreshed));
  return status;
}

void logRegisteredBackends(const BackendRegistry &backends) {
  spdlog::info("Available backends:");
  backends.forEachBackend(
      [](const std::string &name, const BackendWrapper &backend) {
        spdlog::info("Backend: {}, Qubits: {}, Status: {}", name,
                     backend.getNumQubits(),
                     backendStatusLabel(backend.getStatus()));
        for (const auto &format : backend.getSupportedCircuitFormats()) {
          spdlog::info("  Supported format: {}", circuitFormatLabel(format));
        }
      });
}

std::expected<void, Error> selectCompileAndSchedule(
    mqss::QuantumTask &task, const BackendRegistry &backends,
    mqss::mqssci::MQSSCompiler &compiler, TaskScheduler &scheduler,
    BackendSelectionPolicy policy) {
  auto chosenBackend = chooseBackend(task, backends, policy);
  if (!chosenBackend) {
    return std::unexpected(std::move(chosenBackend.error()));
  }
  task.set_scheduled_qpu(*chosenBackend);
  spdlog::info("Selected backend: {}. Compiling task: {}", task.scheduled_qpu(),
               task.task_id());
  return compileAndSchedule(task, backends, compiler, scheduler);
}

std::expected<void, Error>
compileAndSchedule(mqss::QuantumTask &task, const BackendRegistry &backends,
                   mqss::mqssci::MQSSCompiler &compiler,
                   TaskScheduler &scheduler) {
  // Two ways to get here: in the standalone daemon, the backend expired
  // between being chosen and being looked up; in the distributed worker, the
  // selector assigned a backend this process does not hold.
  const auto *backendInfo = backends.find(task.scheduled_qpu());
  if (backendInfo == nullptr) {
    return std::unexpected(Error{.kind = Error::Kind::NoBackendAvailable,
                                 .detail = "Backend '" + task.scheduled_qpu() +
                                           "' is not registered with this "
                                           "process."});
  }

  auto compiled = compileQuantumTask(task, *backendInfo, compiler);
  if (!compiled) {
    return std::unexpected(std::move(compiled.error()));
  }

  spdlog::info("Task {} compiled. Scheduling for execution.", task.task_id());
  scheduler.scheduleTask(task);
  spdlog::debug("Task {} scheduled with priority {}. total scheduled tasks: {}",
                task.task_id(), task.priority(), scheduler.getTaskCount());
  return {};
}

} // namespace mqss::qrmci
