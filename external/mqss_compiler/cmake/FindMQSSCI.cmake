# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(CUDAQ_AUTO_FETCH
    ON
    CACHE BOOL "" FORCE)
set(CATALYST_AUTO_FETCH
    ON
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  mqssci
  GIT_REPOSITORY
    https://github.com/Munich-Quantum-Software-Stack/MQSS-Quantum-Compilation-Suite.git
  GIT_TAG ${QRMCI_MQSSCI_GIT_TAG})
FetchContent_MakeAvailable(mqssci)
