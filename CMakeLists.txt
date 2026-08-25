# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)
project(
  QRMCI
  VERSION 0.1.0
  DESCRIPTION "Quantum Resource Manager and Compiler Infrastructure"
  LANGUAGES C CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(cmake/DependenciesVersion.cmake)

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

find_package(GTest REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost REQUIRED COMPONENTS chrono)
find_package(MQSSCI REQUIRED)
find_package(MQSSScheduler REQUIRED)
find_package(MQSSSubmitter REQUIRED)
find_package(MQSSIntegrationDeploymentFramework REQUIRED)

if(TARGET SimpleAmqpClient AND Rabbitmqc_LIBRARY)
  target_link_libraries(SimpleAmqpClient "${Rabbitmqc_LIBRARY}")
endif()

if(TARGET SimpleAmqpClient AND TARGET Boost::chrono)
  target_link_libraries(SimpleAmqpClient Boost::chrono)
endif()

if(NOT TARGET spdlog::spdlog AND NOT TARGET spdlog::spdlog_header_only)
  find_package(spdlog REQUIRED)
endif()

if(TARGET spdlog::spdlog_header_only)
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog_header_only)
else()
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog)
endif()

include(cmake/Config.cmake)
include(cmake/Warnings.cmake)

configure_file(
  include/qrmci/ConfigDefaults.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/qrmci/ConfigDefaults.h @ONLY)

add_library(qrmci src/BackendWrapper.cpp src/CommunicationHandler.cpp
                  src/Config.cpp src/Runners.cpp)
target_link_libraries(qrmci PUBLIC mqss mqss-ci::mqss-ci mqss_scheduler
                                   mqss_submitter ${QRMCI_SPDLOG_TARGET})
target_include_directories(qrmci PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/"
                                        "${CMAKE_CURRENT_BINARY_DIR}/generated")
qrmci_enable_warnings(qrmci)

option(QRMCI_BUILD_APPS "Build deployment artifacts" ON)
if(QRMCI_BUILD_APPS)
  add_subdirectory(apps/standalone)
endif()

enable_testing()
add_subdirectory(tests)

option(BUILD_QRMCI_DOCS "Build Doxygen API documentation" OFF)
if(BUILD_QRMCI_DOCS)
  add_subdirectory(docs)
endif()
