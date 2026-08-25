# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Central place for FetchContent revisions used across cmake/ and external/*.
# MQSSIntegrationDeploymentFramework and PyQDMI intentionally track development
# branches (active co-development); the rest are pinned to reviewed tags.

set(QRMCI_MQSSCI_GIT_TAG
    "v2.2.0"
    CACHE STRING "MQSS-Quantum-Compilation-Suite revision")
set(QRMCI_MQSS_IDF_GIT_TAG
    "mnf/backend-proto"
    CACHE
      STRING
      "MQSS-Integration-Deployment-Framework revision (tracks branch by design)"
)
set(QRMCI_PYQDMI_GIT_TAG
    "qdmi-v12x"
    CACHE STRING "PyQDMI revision (tracks branch by design)")
set(QRMCI_DOXYGEN_AWESOME_GIT_TAG
    "v2.3.4"
    CACHE STRING "doxygen-awesome-css revision")
