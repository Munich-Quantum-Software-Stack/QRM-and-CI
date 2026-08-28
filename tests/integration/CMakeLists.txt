# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)

add_executable(
  submit_task
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/Config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/CommunicationHandler.cpp
  submit_task.cpp)

target_compile_features(submit_task PRIVATE cxx_std_23)
target_compile_definitions(submit_task PRIVATE FMT_HEADER_ONLY)
qrmci_enable_warnings(submit_task)

target_include_directories(
  submit_task PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                      ${CMAKE_CURRENT_BINARY_DIR}/../generated)

if(TARGET spdlog::spdlog_header_only)
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog_header_only)
else()
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog)
endif()

target_link_libraries(submit_task PRIVATE qrmci)

add_test(NAME submit_task COMMAND submit_task)

add_executable(
  schedule_task
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/Config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/CommunicationHandler.cpp
  schedule_task.cpp)

target_compile_features(schedule_task PRIVATE cxx_std_23)
target_compile_definitions(schedule_task PRIVATE FMT_HEADER_ONLY)
qrmci_enable_warnings(schedule_task)

target_include_directories(
  schedule_task PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                        ${CMAKE_CURRENT_BINARY_DIR}/../generated)

target_link_libraries(schedule_task PRIVATE qrmci)

add_test(NAME schedule_task COMMAND schedule_task)
