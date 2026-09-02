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
    TestCommunicationHandler
    TestBackendRegistry
    TestBackendWrapper
    TestConfig
    TestConstantsMapping
    TestError
    TestLogger)

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

add_custom_command(
  TARGET TestRunnersTaskExecution
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:cxx-qdmi-device> CXX" >
          "${CMAKE_CURRENT_BINARY_DIR}/qdmi.conf"
  VERBATIM
  COMMENT "Generating qdmi.conf for Testing")
set_tests_properties(
  TestRunnersTaskExecution
  PROPERTIES
    ENVIRONMENT
    "QDMI_CONF=${CMAKE_CURRENT_BINARY_DIR}/qdmi.conf;LD_LIBRARY_PATH=$<TARGET_FILE_DIR:qdmi_example_driver>:${LD_LIBRARY_PATH}"
)

# BackendWrapperFromSubmitterTest (in TestBackendWrapper) also drives the real
# example QDMI driver, so it needs the same environment as
# TestRunnersTaskExecution above.
set_tests_properties(
  TestBackendWrapper
  PROPERTIES
    ENVIRONMENT
    "QDMI_CONF=${CMAKE_CURRENT_BINARY_DIR}/qdmi.conf;LD_LIBRARY_PATH=$<TARGET_FILE_DIR:qdmi_example_driver>:${LD_LIBRARY_PATH}"
)
