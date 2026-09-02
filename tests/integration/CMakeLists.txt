# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)

# Client-only integration tests: each sends task(s) to a live QRM&CI deployment
# (apps/standalone or apps/distributed) over a real RabbitMQ/QDMI setup and
# asserts on the result(s) that come back. They require that deployment to
# already be running externally (e.g. via the MQSSIntegrationDeploymentFramework
# docker-compose setup); they are not wired into CI.
set(INTEGRATION_TEST_TARGETS
    submit_task submit_task_distributed submit_task_unsupported_backend
    submit_task_compile_failure submit_task_concurrent)

foreach(target IN LISTS INTEGRATION_TEST_TARGETS)
  add_executable(
    ${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/Config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/CommunicationHandler.cpp
    ${target}.cpp)

  target_compile_features(${target} PRIVATE cxx_std_23)
  target_compile_definitions(${target} PRIVATE FMT_HEADER_ONLY)
  qrmci_enable_warnings(${target})

  target_include_directories(
    ${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                      ${CMAKE_CURRENT_BINARY_DIR}/../generated)

  target_link_libraries(${target} PRIVATE qrmci)

  add_test(NAME ${target} COMMAND ${target})
endforeach()
