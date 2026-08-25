# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)
project(
  MQSSCompiler
  LANGUAGES CXX
  VERSION 1.0.0
  DESCRIPTION "Helper library to use the MQSS Quantum Compilation Suite library"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(MQSSCI REQUIRED)

add_library(mqss_compiler SHARED src/MQSSCompiler.cpp)

add_dependencies(mqss_compiler mqss-ci::mqss-ci)

target_include_directories(mqss_compiler
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
