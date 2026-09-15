# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)

find_package(GTest REQUIRED)

# Client-only integration tests: each spawns and tears down its own daemon(s)
# (apps/standalone or apps/distributed) via DaemonEnvironment, then sends
# task(s) to it over a real RabbitMQ/QDMI setup and asserts on the result(s)
# that come back. `ctest` still must not schedule them by default -- they need a
# reachable RabbitMQ broker and the real example QDMI driver/device -- but no
# daemon needs to be started manually first; each binary owns its own daemon's
# lifecycle.
set(INTEGRATION_TEST_TARGETS
    TestSubmitTask TestSubmitTaskDistributed TestSubmitTaskUnsupportedBackend
    TestSubmitTaskCompileFailure TestSubmitTaskConcurrent TestScheduleTask)

# Shared by every target above: DaemonProcess/DaemonEnvironment own the spawned
# daemon subprocess(es) and the broker-readiness probe. Kept out of the qrmci
# library proper since it depends on GTest, which qrmci itself does not.
add_library(qrmci_integration_test_support STATIC DaemonEnvironment.cpp
                                                  DaemonProcess.cpp)
target_compile_features(qrmci_integration_test_support PRIVATE cxx_std_23)
qrmci_enable_warnings(qrmci_integration_test_support)
target_link_libraries(qrmci_integration_test_support PUBLIC qrmci GTest::GTest)

foreach(target IN LISTS INTEGRATION_TEST_TARGETS)
  add_executable(${target} ${target}.cpp)

  target_compile_features(${target} PRIVATE cxx_std_23)
  qrmci_enable_warnings(${target})

  target_link_libraries(${target} PRIVATE qrmci_integration_test_support)

  set(_daemon_log_root "${CMAKE_CURRENT_BINARY_DIR}/daemon-logs/${target}")

  # Locate the daemon binaries this target spawns via a generator expression
  # (matches the existing FMT_HEADER_ONLY pattern on these same targets); an
  # explicit add_dependencies() below is still required since a genex in
  # target_compile_definitions is string substitution only and creates no
  # build-order edge.
  if(${target} STREQUAL "TestSubmitTaskDistributed")
    target_compile_definitions(
      ${target}
      PRIVATE
        "QRMCI_DISTRIBUTED_SELECTOR_PATH=\"$<TARGET_FILE:qrmcid-distributed-selector>\""
        "QRMCI_DISTRIBUTED_WORKER_PATH=\"$<TARGET_FILE:qrmcid-distributed-worker>\""
        "QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR=\"${_daemon_log_root}/selector\""
        "QRMCI_DISTRIBUTED_WORKER_LOG_DIR=\"${_daemon_log_root}/worker\"")
    add_dependencies(${target} qrmcid-distributed-selector
                     qrmcid-distributed-worker)
  else()
    target_compile_definitions(
      ${target}
      PRIVATE
        "QRMCI_STANDALONE_DAEMON_PATH=\"$<TARGET_FILE:qrmcid-standalone>\""
        "QRMCI_DAEMON_LOG_DIR=\"${_daemon_log_root}\"")
    add_dependencies(${target} qrmcid-standalone)
  endif()

  add_test(NAME ${target} COMMAND ${target})
  # Needs a reachable RabbitMQ broker and the real QDMI example driver, so a
  # plain `ctest` must not schedule it. Run with
  # -DQRMCI_RUN_INTEGRATION_TESTS=ON at configure time, then `ctest -L
  # integration`. QDMI_CONF/LD_LIBRARY_PATH matter here because
  # TestSubmitTaskDistributed's worker (and every other target's
  # qrmcid-standalone) opens the real example QDMI device; DaemonProcess spawns
  # each daemon with this test binary's own environment plus its own
  # QRMCI_LOG_DIR override, so setting these here reaches the spawned daemon(s)
  # too. WORKING_DIRECTORY matters for the same reason as in
  # tests/unit/CMakeLists.txt: the example QDMI driver only opens a config file
  # under its own working directory or $HOME, and a spawned daemon inherits this
  # test process's CWD (DaemonProcess never chdir()s), so it is pointed at the
  # shared qdmi.conf's parent directory explicitly.
  set_tests_properties(
    ${target}
    PROPERTIES
      LABELS
      integration
      TIMEOUT
      120
      DISABLED
      "$<NOT:$<BOOL:${QRMCI_RUN_INTEGRATION_TESTS}>>"
      WORKING_DIRECTORY
      "${CMAKE_CURRENT_BINARY_DIR}/.."
      ENVIRONMENT
      "QDMI_CONF=${QRMCI_TEST_QDMI_CONF};LD_LIBRARY_PATH=$<TARGET_FILE_DIR:qdmi_example_driver>:${LD_LIBRARY_PATH}"
  )
  add_dependencies(${target} qrmci_test_qdmi_conf)
endforeach()

# Every spawned daemon binds the same compiled-in default queue names against
# the same already-running RabbitMQ vhost (none of these targets overrides
# QRMCI_AMQP_*/QRMCI_*_QUEUE), so two of these tests running at once would
# compete as unintended consumers on the same input queues and the same
# simulated QDMI device. This serializes just these six regardless of what else
# `ctest -j` runs alongside them.
set_tests_properties(${INTEGRATION_TEST_TARGETS}
                     PROPERTIES RESOURCE_LOCK qrmci_daemon_rabbitmq)
