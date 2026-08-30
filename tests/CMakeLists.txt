# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)

find_package(GTest REQUIRED)

option(QRMCI_BUILD_UNIT_TESTS
       "Build unit tests" OFF) # currently broken: an LLVM header/library
                               # version mismatch fails linking (undefined
                               # reference to llvm::SmallPtrSetImplBase::
                               # insert_imp), unrelated to the workflow build
option(QRMCI_BUILD_INTEGRATION_TESTS "Build integration tests" ON)

if(QRMCI_BUILD_INTEGRATION_TESTS)
  add_subdirectory(integration)
endif()
if(QRMCI_BUILD_UNIT_TESTS)
  add_subdirectory(unit)
endif()
