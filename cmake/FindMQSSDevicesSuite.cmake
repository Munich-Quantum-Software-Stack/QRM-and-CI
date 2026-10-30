# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(FetchContent)

# MQSS-QDMI-Devices-Suite's own CMakeLists.txt requires BUILD_MODULE to be one
# of lrz/eviden/dcdb (it has no default and fails with a FATAL_ERROR otherwise).
# QRM-and-CI doesn't use any of these backends directly -- it gets its device
# backend from IQMqdmi (see cmake/FindIQMqdmi.cmake) -- so lrz is picked here as
# the lightest option to satisfy the Suite's build.
set(BUILD_MODULE
    "lrz"
    CACHE STRING "Module to build for MQSS-QDMI-Devices-Suite" FORCE)

FetchContent_Declare(
  mqss-devices
  GIT_REPOSITORY
    https://github.com/Munich-Quantum-Software-Stack/MQSS-QDMI-Devices-Suite.git
  GIT_TAG ${QRMCI_MQSS_DEVICES_GIT_TAG}
  SYSTEM)
FetchContent_MakeAvailable(mqss-devices)
