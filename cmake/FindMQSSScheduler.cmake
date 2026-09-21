# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Try to find MQSSScheduler library and headers

include(FetchContent)

FetchContent_Declare(
  mqss_scheduler
  GIT_REPOSITORY ${QRMCI_MQSS_SCHEDULER_REPOSITORY}
  GIT_TAG ${QRMCI_MQSS_SCHEDULER_GIT_TAG})

FetchContent_MakeAvailable(mqss_scheduler)
