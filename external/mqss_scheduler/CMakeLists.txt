# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)
project(
  MQSSScheduler
  LANGUAGES CXX
  VERSION 1.0.0
  DESCRIPTION "MQSS Scheduler")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
endif()

add_library(mqss_scheduler INTERFACE)

target_compile_features(mqss_scheduler INTERFACE cxx_std_20)

target_include_directories(
  mqss_scheduler
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
