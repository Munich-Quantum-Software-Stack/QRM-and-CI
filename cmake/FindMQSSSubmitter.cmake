# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Try to find MQSSSubmitter library and headers

include(FetchContent)

FetchContent_Declare(
  MQSSSubmitter SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/mqss_submitter)

FetchContent_MakeAvailable(MQSSSubmitter)
