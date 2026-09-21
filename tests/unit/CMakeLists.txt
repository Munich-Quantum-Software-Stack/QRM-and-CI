# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)

set(BUILD_QDMI_EXAMPLES
    ON
    CACHE BOOL "" FORCE)

find_package(GTest REQUIRED)

set(TEST_TARGETS
    TestRunnersBackendSelection
    TestRunnersTaskCompilation
    TestRunnersTaskExecution
    TestTaskSubmission
    TestCommunicationHandler
    TestBackendRegistry
    TestBackendWrapper
    TestCircuitFormatPolicy
    TestDaemon
    TestConfig
    TestConstantsMapping
    TestError
    TestLogger
    TestPublicationThrottle)

foreach(target IN LISTS TEST_TARGETS)
  add_executable(${target} ${target}.cpp)

  target_compile_features(${target} PRIVATE cxx_std_23)
  qrmci_enable_warnings(${target})

  target_include_directories(
    ${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                      ${CMAKE_CURRENT_BINARY_DIR}/../generated)

  target_link_libraries(${target} PRIVATE qrmci GTest::GTest GTest::Main)

  add_test(NAME ${target} COMMAND ${target})
endforeach()

target_include_directories(TestTaskSubmission PRIVATE ${PROJECT_SOURCE_DIR}/src)

# DaemonProcess.{h,cpp} (tests/integration) has no qrmci dependency of its own
# -- it is a standalone posix_spawn/waitpid wrapper -- so its ownership contract
# is exercised here as a plain process test against /bin/true and /bin/sleep,
# without a RabbitMQ broker or QDMI device and without pulling in
# qrmci_integration_test_support, which tests/integration only builds when
# QRMCI_BUILD_INTEGRATION_TESTS and QRMCI_BUILD_APPS are both enabled.
add_executable(
  TestDaemonProcess ${CMAKE_CURRENT_SOURCE_DIR}/../integration/DaemonProcess.cpp
                    TestDaemonProcess.cpp)
target_compile_features(TestDaemonProcess PRIVATE cxx_std_23)
qrmci_enable_warnings(TestDaemonProcess)
target_include_directories(TestDaemonProcess
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../integration)
target_link_libraries(TestDaemonProcess PRIVATE GTest::GTest GTest::Main)
add_test(NAME TestDaemonProcess COMMAND TestDaemonProcess)

# The shipped example TOML files are the only place a real deployment's config
# actually gets validated against the schema before this test suite runs, so
# TestConfig loads each one directly rather than trusting the committed contents
# to stay in sync by hand.
target_compile_definitions(
  TestConfig
  PRIVATE
    QRMCI_EXAMPLE_CONFIG_FULL_REFERENCE="${CMAKE_SOURCE_DIR}/apps/qrmci.full-reference.example.toml"
    QRMCI_EXAMPLE_CONFIG_STANDALONE="${CMAKE_SOURCE_DIR}/apps/standalone/config/qrmci-standalone.example.toml"
    QRMCI_EXAMPLE_CONFIG_DISTRIBUTED_WORKER="${CMAKE_SOURCE_DIR}/apps/distributed/config/qrmci-distributed-worker.example.toml"
    QRMCI_EXAMPLE_CONFIG_DISTRIBUTED_SELECTOR="${CMAKE_SOURCE_DIR}/apps/distributed/config/qrmci-distributed-selector.example.toml"
)

# qdmi.conf comes from qrmci_test_qdmi_conf / QRMCI_TEST_QDMI_CONF in
# tests/CMakeLists.txt, shared with tests/integration.

# The tests that drive the real example QDMI driver: they need qdmi.conf and the
# driver on the library path. Every one of them must depend on the target that
# writes the file, so a targeted build of any single one still produces it.
set(QDMI_DRIVEN_TEST_TARGETS TestRunnersTaskExecution TestDaemon)

foreach(target IN LISTS QDMI_DRIVEN_TEST_TARGETS)
  add_dependencies(${target} qrmci_test_qdmi_conf)
  # Client::openDevice() takes a filesystem path and existence-checks it before
  # dlopen, so the driver has to be named by path; LD_LIBRARY_PATH alone will
  # not find it.
  target_compile_definitions(
    ${target}
    PRIVATE QRMCI_TEST_QDMI_DRIVER_PATH="$<TARGET_FILE:qdmi_example_driver>")
  # The example QDMI driver only opens a config file under its own working
  # directory or $HOME (see Is_path_allowed() in qdmi_example_driver.cpp), and
  # ctest's default working directory is this subdirectory's binary dir, one
  # level below where the shared qdmi.conf lives. Point it at that parent.
  set_tests_properties(
    ${target}
    PROPERTIES
      WORKING_DIRECTORY
      "${CMAKE_CURRENT_BINARY_DIR}/.."
      ENVIRONMENT
      "QDMI_CONF=${QRMCI_TEST_QDMI_CONF};LD_LIBRARY_PATH=$<TARGET_FILE_DIR:qdmi_example_driver>:${LD_LIBRARY_PATH}"
  )
endforeach()
