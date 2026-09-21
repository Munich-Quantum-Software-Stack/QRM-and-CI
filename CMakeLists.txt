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

# Right after project(), as CMake's own documentation advises, so that any
# dependency pulled in below can register tests whatever order it is brought in.
# No dependency does so today -- MQSS-Submitter's tests are forced off in
# cmake/FindMQSSSubmitter.cmake -- but nothing here has to depend on that
# staying true.
enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# mqt-core (fetched transitively via MQSSCI/MQT-QMAP below) force-enables
# CMAKE_INTERPROCEDURAL_OPTIMIZATION (LTO). With LTO on, GCC fails to externally
# emit weak/inline LLVM ADT symbols (e.g.
# llvm::SmallPtrSetImplBase::insert_imp), so any target that pulls in the
# prebuilt LLVM/MLIR libraries fails to link with "undefined reference". Force
# it off for the whole build tree.
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
# cmake/FindMQSSSubmitter.cmake fetches MQSSSubmitter (plain FetchContent). It
# must run before find_package(MQSSCI) below: MQSSSubmitter declares the QDMI
# content names it needs for its per-version adapters, and FetchContent is
# first-declaration-wins, so a later MQSSCI declaration of the same name would
# otherwise decide the revision for both.
find_package(MQSSSubmitter REQUIRED)

# Hoisted ahead of add_subdirectory(tests) (tests/CMakeLists.txt declares the
# same option) because cmake/QdmiExampleDriver.cmake, included immediately
# below, needs to know before find_package(MQSSCI) runs whether QRM&CI's own
# unit tests will need a QDMI example driver of their own. option() is
# idempotent, so tests/CMakeLists.txt re-declaring it later is harmless.
option(QRMCI_BUILD_UNIT_TESTS "Build unit tests" ON)
include(cmake/QdmiExampleDriver.cmake)

find_package(MQSSCI REQUIRED)
find_package(MQSSScheduler REQUIRED)
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

configure_file(
  include/qrmci/ConfigDefaults.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/qrmci/ConfigDefaults.h @ONLY)

file(GLOB_RECURSE QRMCI_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Explicit STATIC: qrmci must not follow the ambient BUILD_SHARED_LIBS default.
# Both cmake/QdmiExampleDriver.cmake above and MQSSCI's own cmake/Findqdmi.cmake
# temporarily force BUILD_SHARED_LIBS=ON around their respective QDMI fetches
# and restore whatever value they observed beforehand -- if that observed value
# was itself already leaked ON by an earlier such fetch (e.g. MQSSSubmitter's),
# the restore faithfully preserves the leak instead of fixing it, leaving
# BUILD_SHARED_LIBS=ON for the rest of the configure. A `qrmci` built as a
# shared library then has to statically embed its own copy of every LLVM/MLIR
# static archive MQSSCI's exported targets pull in PUBLIC, duplicating the copy
# already inside MQSSCI's own .so's -- LLVM's global cl::opt registry aborts at
# process start when two independently-embedded copies both register the same
# option (e.g. `-opt-bisect-limit`).
add_library(qrmci STATIC ${QRMCI_SOURCES})
target_link_libraries(
  qrmci PUBLIC mqss mqss-ci::mqss-ci scheduler mqss_submitter
               ${QRMCI_SPDLOG_TARGET} tomlplusplus::tomlplusplus)
target_include_directories(qrmci PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/"
                                        "${CMAKE_CURRENT_BINARY_DIR}/generated")
# TOML_EXCEPTIONS=0 selects toml++'s non-throwing parse API (parse_result with
# operator bool()/.error()), matching this codebase's std::expected
# error-handling convention. Target-wide so every qrmci translation unit that
# includes toml++ agrees on the ABI, not just src/Config.cpp.
target_compile_definitions(qrmci PRIVATE TOML_EXCEPTIONS=0)
# PUBLIC, so every consumer's own link line gets it too: MQSSCI's exported
# targets mix statically-linked LLVM/MLIR/CUDA-Q archives (e.g. libCCDialect.a)
# with the shared libraries that already embed most of the same symbols (e.g.
# libMQSSSupportedDialects.so). Toolchains that default to `--as-needed`
# (Ubuntu's GCC does) can drop one of those shared libraries as unneeded at the
# point it's encountered, then fail to resolve a later archive member's
# reference to a symbol that library would have provided (observed with
# libCCDialect.a needing a TypeID symbol libMQSSSupportedDialects.so exports,
# "DSO missing from command line"). --no-as-needed keeps every explicitly listed
# library linked regardless of encounter order. Only applied where the linker
# actually supports it, so non-GNU-compatible linkers configure instead of
# failing at option parsing.
include(CheckLinkerFlag)
check_linker_flag(CXX "LINKER:--no-as-needed"
                  QRMCI_LINKER_SUPPORTS_NO_AS_NEEDED)
if(QRMCI_LINKER_SUPPORTS_NO_AS_NEEDED)
  target_link_options(qrmci PUBLIC "LINKER:--no-as-needed")
endif()
qrmci_enable_warnings(qrmci)

option(QRMCI_BUILD_APPS "Build deployment artifacts" ON)
if(QRMCI_BUILD_APPS)
  add_subdirectory(apps/standalone)
  add_subdirectory(apps/distributed)
endif()

add_subdirectory(tests)

# Static regression check: apps/distributed/.env.example must point the shared
# external broker at the Docker host alias, not container loopback.
add_test(
  NAME CheckDistributedExampleHost
  COMMAND
    "${CMAKE_COMMAND}"
    "-DENV_EXAMPLE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/apps/distributed/.env.example"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/CheckDistributedExampleHost.cmake")
set_tests_properties(CheckDistributedExampleHost PROPERTIES LABELS "static")

option(BUILD_QRMCI_DOCS "Build Doxygen API documentation" OFF)
if(BUILD_QRMCI_DOCS)
  add_subdirectory(docs)
endif()
