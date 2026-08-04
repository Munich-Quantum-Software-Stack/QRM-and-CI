# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(ExternalProject)

set(MQSSCI_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/mqssci-build)

ExternalProject_Add(
  mqssci_external
  GIT_REPOSITORY
    https://github.com/Munich-Quantum-Software-Stack/MQSS-Quantum-Compilation-Suite.git
  GIT_TAG v2.0.0
  PREFIX ${CMAKE_BINARY_DIR}/_deps/mqssci-build
  SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/mqssci-src
  BINARY_DIR ${CMAKE_BINARY_DIR}/_deps/mqssci-src # in-source make, adjust if
                                                  # out-of-source supported
  CONFIGURE_COMMAND "" # no configure step needed for plain make
  BUILD_COMMAND bash -c "make build"
  INSTALL_COMMAND make target
  # BUILD_IN_SOURCE   TRUE  # set if module-Y's Makefile expects to run from its
  # source root
)

set(MQSSCI_SRC_DIR ${CMAKE_BINARY_DIR}/_deps/mqssci-src)
