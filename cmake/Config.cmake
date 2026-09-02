# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Dynamic defaults generated via cmake build-system context

# Logging
set(QRMCI_LOG_DIR
    "${CMAKE_BINARY_DIR}/logs"
    CACHE PATH "Default log directory")

# spdlog target: prefer the header-only variant when it was resolved by
# find_package(spdlog) in the top-level CMakeLists.txt.
if(TARGET spdlog::spdlog_header_only)
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog_header_only)
else()
  set(QRMCI_SPDLOG_TARGET spdlog::spdlog)
endif()
