# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(FetchContent)

FetchContent_Declare(
  MQSSIntegrationDeploymentFramework
  GIT_REPOSITORY
    git@github.com:Munich-Quantum-Software-Stack/MQSS-Integration-Deployment-Framework.git
  GIT_TAG ${QRMCI_MQSS_IDF_GIT_TAG})

FetchContent_MakeAvailable(MQSSIntegrationDeploymentFramework)
