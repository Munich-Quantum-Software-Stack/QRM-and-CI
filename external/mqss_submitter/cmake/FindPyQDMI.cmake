# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(FetchContent)

FetchContent_Declare(
  PyQDMI
  GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/PyQDMI.git
  GIT_TAG qdmi-v12x)

FetchContent_MakeAvailable(PyQDMI)

target_include_directories(objqdmi_cpp PUBLIC ${pyqdmi_SOURCE_DIR}/include
                                              ${QDMI_INCLUDE_BUILD_DIR})

target_include_directories(qdmi_cpp_shared PUBLIC ${pyqdmi_SOURCE_DIR}/include
                                                  ${QDMI_INCLUDE_BUILD_DIR})

target_include_directories(qdmi_cpp_static PUBLIC ${pyqdmi_SOURCE_DIR}/include
                                                  ${QDMI_INCLUDE_BUILD_DIR})
