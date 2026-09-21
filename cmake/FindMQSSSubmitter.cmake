# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(FetchContent)

# QRM&CI's own MQSS Integration & Deployment Framework must be configured first:
# MQSSSubmitter's own fetch of that same project defers to whatever already
# provides mqss::mqss, and this is what provides it.
find_package(MQSSIntegrationDeploymentFramework REQUIRED)

# MQSSSubmitter's own tests and tools are never part of this build: they fetch
# GoogleTest, build seven per-version QDMI driver fixtures, and register ctest
# entries that would land in QRM&CI's own test list.
set(MQSS_SUBMITTER_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(MQSS_SUBMITTER_BUILD_TOOLS
    OFF
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  mqss_submitter
  GIT_REPOSITORY ${QRMCI_MQSS_SUBMITTER_REPOSITORY}
  GIT_TAG ${QRMCI_MQSS_SUBMITTER_GIT_TAG}
  SYSTEM)

FetchContent_MakeAvailable(mqss_submitter)
