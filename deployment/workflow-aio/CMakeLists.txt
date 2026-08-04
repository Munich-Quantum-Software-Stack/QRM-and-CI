# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)
project(
  QRMCI_AIO_DAEMON
  VERSION 0.1.0
  DESCRIPTION
    "Quantum Resource Manager and Compiler Infrastructure Daemon All In One"
  LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
endif()

find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)

if(TARGET spdlog::spdlog_header_only)
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog_header_only)
else()
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog)
endif()

add_executable("qrmci_aio_daemon" main.cpp)

target_link_libraries("qrmci_aio_daemon" PUBLIC qrmci Threads::Threads
                                                ${QRMCI_SPDLOG_TARGET})

target_include_directories(
  "qrmci_aio_daemon" PUBLIC "${CMAKE_SOURCE_DIR}/include"
                            "${CMAKE_BINARY_DIR}/generated")
