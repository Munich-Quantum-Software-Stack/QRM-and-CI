# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)
project(
  MQSSSubmitter
  LANGUAGES CXX
  VERSION 1.0.0
  DESCRIPTION
    "MQSS Submitter Library to submit quantum tasks to a backend using QDMI")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
endif()

# find_package(PyQDMI REQUIRED)
include(cmake/FindPyQDMI.cmake)

add_library(mqss_submitter SHARED src/Submitter.cpp)

target_link_libraries(mqss_submitter PUBLIC qdmi_cpp)

target_include_directories(mqss_submitter
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
