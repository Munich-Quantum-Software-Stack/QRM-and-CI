# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)
project(
  QRMCI
  VERSION 0.1.0
  DESCRIPTION "Quantum Resource Manager and Compiler Infrastructure"
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

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Boost 1.89+ no longer ships a separate boost_system package config, but some
# third-party dependency trees still request it explicitly. Provide a local shim
# only for this build tree instead of overriding find_package globally.
set(_qrm_boost_system_shim_dir
    "${CMAKE_BINARY_DIR}/_deps/cmake/boost_system-shim")
file(MAKE_DIRECTORY "${_qrm_boost_system_shim_dir}")
file(
  WRITE "${_qrm_boost_system_shim_dir}/boost_systemConfig.cmake"
  "if(NOT TARGET Boost::system)\n  add_library(Boost::system INTERFACE IMPORTED)\nendif()\n"
)
file(
  WRITE "${_qrm_boost_system_shim_dir}/boost_systemConfigVersion.cmake"
  "set(PACKAGE_VERSION \"1.90.0\")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\nset(PACKAGE_VERSION_EXACT TRUE)\n"
)
list(PREPEND CMAKE_PREFIX_PATH "${_qrm_boost_system_shim_dir}")

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(MQSSCompiler REQUIRED)
find_package(MQSSScheduler REQUIRED)
find_package(MQSSSubmitter REQUIRED)
find_package(MQSSIntegrationDeploymentFramework REQUIRED)

if(TARGET spdlog::spdlog_header_only)
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog_header_only)
else()
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog)
endif()

include(cmake/Config.cmake)

configure_file(include/ConfigDefaults.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/ConfigDefaults.hpp @ONLY)

# Add the include paths
include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/include"
                    "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_library(qrmci src/CommunicationHandler.cpp src/Config.cpp src/Runners.cpp)
target_link_libraries(qrmci PUBLIC mqss mqss_compiler mqss_scheduler
                                   mqss_submitter ${QRMCI_SPDLOG_TARGET})

add_subdirectory(deployment/workflow-aio)

enable_testing()
add_subdirectory(tests)

option(BUILD_QRMCI_DOCS "Build Doxygen API documentation" OFF)
if(BUILD_QRMCI_DOCS)
  add_subdirectory(docs)
endif()

# Enable FetchContent for integration
include(FetchContent)
