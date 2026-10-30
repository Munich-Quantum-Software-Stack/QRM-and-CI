# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(FetchContent)

# cpr would otherwise build its pinned curl 8.13.0, which does not compile
# against OpenSSL 4 (opaque ASN1_STRING); use the system libcurl instead.
set(CPR_USE_SYSTEM_CURL
    ON
    CACHE BOOL "Use system curl for CPR")

FetchContent_Declare(
  iqm-qdmi
  GIT_REPOSITORY ${IQM_QDMI_REPOSITORY}
  GIT_TAG ${IQM_QDMI_DEVICE_GIT_TAG}
  SYSTEM)
FetchContent_MakeAvailable(iqm-qdmi)
