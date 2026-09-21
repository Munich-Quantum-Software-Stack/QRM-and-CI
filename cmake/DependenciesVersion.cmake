# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Central place for FetchContent revisions used across cmake/ and external/*.
# MQSSIntegrationDeploymentFramework intentionally tracks a development branch
# (active co-development); the rest are pinned to reviewed tags.

set(QRMCI_MQSS_SCHEDULER_GIT_TAG
    "mc/sscheduler"
    CACHE STRING "MQSS-Scheduler revision (tracks branch by design)")
set(QRMCI_MQSS_SCHEDULER_REPOSITORY
    "https://github.com/Munich-Quantum-Software-Stack/MQSS-Scheduler.git"
    CACHE STRING "Upstream MQSS-Scheduler repository")
set(QRMCI_MQSSCI_GIT_TAG
    "v2.2.1"
    CACHE STRING "MQSS-Quantum-Compilation-Suite revision")
set(QRMCI_MQSS_IDF_GIT_TAG
    "mnf/protobuf-json-fix"
    CACHE
      STRING
      "MQSS-Integration-Deployment-Framework revision (tracks branch by design)"
)
set(QRMCI_MQSS_IDF_REPOSITORY
    "https://github.com/Munich-Quantum-Software-Stack/MQSS-Integration-Deployment-Framework.git"
    CACHE STRING "Upstream MQSS-Integration-Deployment-Framework repository")

# Never interpolate a credential into this URL: it becomes part of the cached
# CMakeCache.txt string and the fetched repository's .git/config. Authenticate
# locally via a process-scoped git config override (see
# docs/development-guide.md) or a git credential helper instead.
set(QRMCI_MQSS_SUBMITTER_REPOSITORY
    "https://github.com/Munich-Quantum-Software-Stack/MQSS-Submitter.git"
    CACHE STRING "Upstream MQSSSubmitter repository")
set(QRMCI_MQSS_SUBMITTER_GIT_TAG
    "mnf/qrmci-redesign"
    CACHE STRING "MQSSSubmitter revision")

# MQSS-Submitter populates QDMI headers only, without add_subdirectory, so it
# leaves no built cxx-qdmi-device/qdmi_example_driver behind for QRM&CI's own
# QDMI-driven unit tests to open. cmake/QdmiExampleDriver.cmake builds one from
# this pin instead. v1.3.3 is the newest spec version MQSS-Submitter supports.
set(QDMI_REPOSITORY
    "https://github.com/Munich-Quantum-Software-Stack/QDMI.git"
    CACHE STRING "Upstream QDMI repository")
set(QRMCI_QDMI_GIT_TAG
    "v1.3.3"
    CACHE STRING "QDMI revision for QRM&CI's own example driver/device fixture")

set(QRMCI_DOXYGEN_AWESOME_GIT_TAG
    "v2.3.4"
    CACHE STRING "doxygen-awesome-css revision")
set(QRMCI_TOMLPLUSPLUS_GIT_TAG
    "v3.4.0"
    CACHE STRING "toml++ revision")
