# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)

# Connects to a real LRZ backend over the network -- deliberately never
# registered with add_test(), so it cannot run via `ctest` under any label or
# filter. Built and invoked only through run.sh.
add_executable(
  TestLRZWorker ${CMAKE_SOURCE_DIR}/tests/integration/DaemonProcess.cpp
                TestLRZWorker.cpp)
target_compile_features(TestLRZWorker PRIVATE cxx_std_23)
qrmci_enable_warnings(TestLRZWorker)

# Reuses DaemonProcess.{h,cpp} from tests/integration rather than duplicating
# it: it has no GTest/qrmci dependency of its own (plain posix_spawn/waitpid
# wrapper), so pulling it in here adds no framework coupling.
target_include_directories(TestLRZWorker
                           PRIVATE ${CMAKE_SOURCE_DIR}/tests/integration)

target_link_libraries(TestLRZWorker PRIVATE qrmci qrmci_test_circuit_fixtures)

set(real_backend_daemon_log_root "${CMAKE_CURRENT_BINARY_DIR}/daemon-logs")

# LRZ/CMakeLists.txt
set(QRMCI_LRZ_QDMI_CONF "${CMAKE_CURRENT_BINARY_DIR}/lrz_qdmi.conf")
add_custom_command(
  OUTPUT "${QRMCI_LRZ_QDMI_CONF}"
  COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:lrz_device> LRZ" >
          "${QRMCI_LRZ_QDMI_CONF}"
  DEPENDS lrz_device
  VERBATIM
  COMMENT "Generating lrz_qdmi.conf for TestLRZWorker")
add_custom_target(qrmci_lrz_qdmi_conf DEPENDS "${QRMCI_LRZ_QDMI_CONF}")

target_compile_definitions(
  TestLRZWorker
  PRIVATE
    "QRMCI_DISTRIBUTED_SELECTOR_PATH=\"$<TARGET_FILE:qrmcid-distributed-selector>\""
    "QRMCI_DISTRIBUTED_WORKER_PATH=\"$<TARGET_FILE:qrmcid-distributed-worker>\""
    "QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR=\"${real_backend_daemon_log_root}/selector\""
    "QRMCI_DISTRIBUTED_WORKER_B_LOG_DIR=\"${real_backend_daemon_log_root}/worker-lrz\""
    "QRMCI_SELECTOR_TEST_CONFIG=\"${CMAKE_CURRENT_SOURCE_DIR}/config/selector-test-config.toml\""
    "QRMCI_WORKER_LRZ_TEST_CONFIG=\"${CMAKE_CURRENT_SOURCE_DIR}/config/worker-lrz-test-config.toml\""
    "QRMCI_QDMI_EXAMPLE_DRIVER_PATH=\"$<TARGET_FILE:qdmi_example_driver>\""
    "QRMCI_WORKER_LRZ_QDMI_CONF=\"${QRMCI_LRZ_QDMI_CONF}\"")

add_dependencies(TestLRZWorker qrmci_lrz_qdmi_conf qrmcid-distributed-selector
                 qrmcid-distributed-worker)
