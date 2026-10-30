# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(FetchContent)

FetchContent_Declare(
  aws-qdmi
  GIT_REPOSITORY ${AWS_QDMI_REPOSITORY}
  GIT_TAG ${AWS_QDMI_DEVICE_GIT_TAG}
  SYSTEM)
FetchContent_MakeAvailable(aws-qdmi)
