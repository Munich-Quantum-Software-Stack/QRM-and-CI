# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)
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

# mqt-core (fetched transitively via MQSSCI/MQT-QMAP below) force-enables
# CMAKE_INTERPROCEDURAL_OPTIMIZATION (LTO). With LTO on, GCC fails to externally
# emit weak/inline LLVM ADT symbols (e.g.
# llvm::SmallPtrSetImplBase::insert_imp), causing "undefined reference" link
# failures in any target that pulls in the prebuilt LLVM/MLIR
set(ENABLE_IPO
    OFF
    CACHE BOOL "Disable IPO/LTO globally" FORCE)

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
# CMake 3.30+ removed the bundled FindBoost module in favor of Boost's own
# config-mode package files; guarded for compatibility with older CMake versions
# that don't know this policy yet.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost REQUIRED COMPONENTS chrono)
find_package(MQSSCI REQUIRED)
find_package(MQSSScheduler REQUIRED)
find_package(MQSSSubmitter REQUIRED)
find_package(MQSSIntegrationDeploymentFramework REQUIRED)
find_package(TomlPlusPlus REQUIRED)

if(TARGET SimpleAmqpClient AND Rabbitmqc_LIBRARY)
  target_link_libraries(SimpleAmqpClient "${Rabbitmqc_LIBRARY}")
endif()

if(TARGET SimpleAmqpClient AND TARGET Boost::chrono)
  target_link_libraries(SimpleAmqpClient Boost::chrono)
endif()

if(NOT TARGET spdlog::spdlog AND NOT TARGET spdlog::spdlog_header_only)
  find_package(spdlog REQUIRED)
endif()

include(cmake/Config.cmake)
include(cmake/Warnings.cmake)

qrmci_treat_as_system(mqss)
qrmci_treat_as_system(MQSSCIInterfaces)
qrmci_treat_as_system(MQSSCIPasses)
qrmci_treat_as_system(mqss_scheduler)
qrmci_treat_as_system(mqss_submitter)

configure_file(
  include/qrmci/ConfigDefaults.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/qrmci/ConfigDefaults.h @ONLY)

file(GLOB_RECURSE QRMCI_SOURCES "src/*.cpp")

add_library(qrmci ${QRMCI_SOURCES})
target_link_libraries(
  qrmci PUBLIC mqss mqss-ci::mqss-ci scheduler mqss_submitter
               ${QRMCI_SPDLOG_TARGET} tomlplusplus::tomlplusplus)
target_include_directories(qrmci PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/"
                                        "${CMAKE_CURRENT_BINARY_DIR}/generated")
qrmci_enable_warnings(qrmci)

option(QRMCI_BUILD_APPS "Build deployment artifacts" ON)
if(QRMCI_BUILD_APPS)
  add_subdirectory(apps/standalone)
  add_subdirectory(apps/distributed)
endif()

enable_testing()
add_subdirectory(tests)

option(BUILD_QRMCI_DOCS "Build Doxygen API documentation" OFF)
if(BUILD_QRMCI_DOCS)
  add_subdirectory(docs)
endif()
