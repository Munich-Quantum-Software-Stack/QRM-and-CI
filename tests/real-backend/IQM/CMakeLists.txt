# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)

# Connects to a real IQM backend over the network -- deliberately never
# registered with add_test(), so it cannot run via `ctest` under any label or
# filter. Built and invoked only through run.sh.
add_executable(
  TestIQMWorker ${CMAKE_SOURCE_DIR}/tests/integration/DaemonProcess.cpp
                TestIQMWorker.cpp)
target_compile_features(TestIQMWorker PRIVATE cxx_std_23)
qrmci_enable_warnings(TestIQMWorker)

# Reuses DaemonProcess.{h,cpp} from tests/integration rather than duplicating
# it: it has no GTest/qrmci dependency of its own (plain posix_spawn/waitpid
# wrapper), so pulling it in here adds no framework coupling.
target_include_directories(TestIQMWorker
                           PRIVATE ${CMAKE_SOURCE_DIR}/tests/integration)

target_link_libraries(TestIQMWorker PRIVATE qrmci qrmci_test_circuit_fixtures)

set(real_backend_daemon_log_root "${CMAKE_CURRENT_BINARY_DIR}/daemon-logs")

target_compile_definitions(
  TestIQMWorker
  PRIVATE
    "QRMCI_DISTRIBUTED_SELECTOR_PATH=\"$<TARGET_FILE:qrmcid-distributed-selector>\""
    "QRMCI_DISTRIBUTED_WORKER_PATH=\"$<TARGET_FILE:qrmcid-distributed-worker>\""
    "QRMCI_DISTRIBUTED_SELECTOR_LOG_DIR=\"${real_backend_daemon_log_root}/selector\""
    "QRMCI_DISTRIBUTED_WORKER_B_LOG_DIR=\"${real_backend_daemon_log_root}/worker-iqm\""
    "QRMCI_SELECTOR_TEST_CONFIG=\"${CMAKE_CURRENT_SOURCE_DIR}/config/selector-test-config.toml\""
    "QRMCI_WORKER_IQM_TEST_CONFIG=\"${CMAKE_CURRENT_SOURCE_DIR}/config/worker-iqm-test-config.toml\""
    "QRMCI_WORKER_IQM_QDMI_CONF=\"${CMAKE_SOURCE_DIR}/tests/input/iqm_qdmi.conf\""
)

add_dependencies(TestIQMWorker qrmcid-distributed-selector
                 qrmcid-distributed-worker)
