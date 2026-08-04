# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Try to find MQSSCompiler library and headers

include(FetchContent)

FetchContent_Declare(
  MQSSCompiler SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/mqss_compiler)

FetchContent_MakeAvailable(MQSSCompiler)
