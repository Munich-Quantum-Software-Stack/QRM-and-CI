# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)

find_package(GTest REQUIRED)

option(QRMCI_BUILD_UNIT_TESTS "Build unit tests" ON)
option(QRMCI_BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(
  QRMCI_BUILD_REAL_BACKEND_TESTS
  "Build real-backend tests (connect to a live, real quantum backend; never registered with ctest)"
  OFF)

option(
  QRMCI_RUN_INTEGRATION_TESTS
  "Register integration tests for ctest (requires a live QRM&CI deployment)"
  OFF)

# qdmi.conf gets its own target rather than a POST_BUILD step on one test,
# because tests in both subdirectories need it: the QDMI-driven unit tests, and
# the daemons the integration binaries spawn. Declared here, ahead of both
# add_subdirectory() calls, so either one can depend on it whichever runs first.
set(QRMCI_TEST_QDMI_CONF "${CMAKE_CURRENT_BINARY_DIR}/qdmi.conf")
add_custom_command(
  OUTPUT "${QRMCI_TEST_QDMI_CONF}"
  COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:cxx-qdmi-device> CXX" >
          "${QRMCI_TEST_QDMI_CONF}"
  DEPENDS cxx-qdmi-device
  VERBATIM
  COMMENT "Generating qdmi.conf for Testing")
add_custom_target(qrmci_test_qdmi_conf DEPENDS "${QRMCI_TEST_QDMI_CONF}")

# The sample circuits under fixtures/circuits are read from their source
# directory at run time rather than copied next to the binaries: the path is
# baked in as a compile definition so a test finds them whatever working
# directory ctest gives it. Declared ahead of both add_subdirectory() calls
# because the unit and integration suites share the same circuits.
add_library(qrmci_test_circuit_fixtures INTERFACE)
target_include_directories(qrmci_test_circuit_fixtures
                           INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_definitions(
  qrmci_test_circuit_fixtures
  INTERFACE
    QRMCI_TEST_CIRCUIT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/circuits"
)

if(QRMCI_BUILD_INTEGRATION_TESTS)
  if(QRMCI_BUILD_APPS)
    add_subdirectory(integration)
  else()
    message(
      WARNING
        "QRMCI_BUILD_INTEGRATION_TESTS is ON but QRMCI_BUILD_APPS is OFF -- "
        "skipping tests/integration, since its tests spawn the daemon "
        "binaries QRMCI_BUILD_APPS builds.")
  endif()
endif()
if(QRMCI_BUILD_UNIT_TESTS)
  add_subdirectory(unit)
endif()
if(QRMCI_BUILD_REAL_BACKEND_TESTS)
  if(QRMCI_BUILD_APPS)
    add_subdirectory(real-backend)
  else()
    message(
      WARNING
        "QRMCI_BUILD_REAL_BACKEND_TESTS is ON but QRMCI_BUILD_APPS is OFF -- "
        "skipping tests/real-backend, since its test spawns the daemon "
        "binaries QRMCI_BUILD_APPS builds.")
  endif()
endif()
