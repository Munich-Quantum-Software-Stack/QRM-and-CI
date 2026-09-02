# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)
project(
  QRMCI_DISTRIBUTED_DAEMONS
  VERSION 0.1.0
  DESCRIPTION
    "Quantum Resource Manager and Compiler Infrastructure Distributed Daemons"
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

add_executable("qrmcid-distributed-selector" selector/main.cpp)
qrmci_enable_warnings(qrmcid-distributed-selector)

target_link_libraries("qrmcid-distributed-selector"
                      PUBLIC qrmci Threads::Threads ${QRMCI_SPDLOG_TARGET})

target_include_directories(
  "qrmcid-distributed-selector" PUBLIC "${CMAKE_SOURCE_DIR}/include"
                                       "${CMAKE_BINARY_DIR}/generated")

add_executable("qrmcid-distributed-worker" worker/main.cpp)
qrmci_enable_warnings(qrmcid-distributed-worker)

target_link_libraries("qrmcid-distributed-worker" PUBLIC qrmci Threads::Threads
                                                         ${QRMCI_SPDLOG_TARGET})

target_include_directories(
  "qrmcid-distributed-worker" PUBLIC "${CMAKE_SOURCE_DIR}/include"
                                     "${CMAKE_BINARY_DIR}/generated")
