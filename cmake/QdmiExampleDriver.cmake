# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The unit tests listed in tests/unit/CMakeLists.txt as QDMI_DRIVEN_TEST_TARGETS
# open a real QDMI example driver -- cxx-qdmi-device / qdmi_example_driver --
# rather than a fake. MQSSSubmitter's own QDMI fetch normally leaves that driver
# behind as a side effect, so this file is a fallback for the case where it has
# not: it fetches a QDMI example driver of QRM&CI's own so those tests can run
# either way.
#
# Guarded on `NOT TARGET cxx-qdmi-device`, so it is a no-op whenever
# MQSSSubmitter already declared `qdmi`. That guard matters: FetchContent is
# first-declaration-wins, so two declarations of the same name would let
# whichever ran first silently decide the revision for both.
if(QRMCI_BUILD_UNIT_TESTS AND NOT TARGET cxx-qdmi-device)
  include(FetchContent)

  # Same option set MQSS-Submitter's own cmake/FindQDMI.cmake forces on its own
  # fetch, for the same reason: an examples build with the shared example
  # device, nothing else.
  set(BUILD_QDMI_EXAMPLES
      ON
      CACHE BOOL "" FORCE)
  set(BUILD_QDMI_TESTS
      OFF
      CACHE BOOL "" FORCE)
  set(BUILD_QDMI_TEMPLATES
      OFF
      CACHE BOOL "" FORCE)
  set(BUILD_QDMI_DOCS
      OFF
      CACHE BOOL "" FORCE)

  # Save/restore BUILD_SHARED_LIBS around the fetch, including whether it was
  # even defined beforehand -- see the matching comment in MQSS-Submitter's own
  # cmake/FindQDMI.cmake for why a plain restore would be unsafe (it would leave
  # a defined-but-empty cache entry behind when the variable started out
  # undefined).
  if(DEFINED BUILD_SHARED_LIBS)
    set(_qrmci_qdmi_had_shared_libs TRUE)
    set(_qrmci_qdmi_saved_shared_libs ${BUILD_SHARED_LIBS})
  else()
    set(_qrmci_qdmi_had_shared_libs FALSE)
  endif()
  set(BUILD_SHARED_LIBS
      ON
      CACHE BOOL "" FORCE)

  # QDMI_REPOSITORY and QRMCI_QDMI_GIT_TAG come from
  # cmake/DependenciesVersion.cmake, included earlier in root CMakeLists.txt.
  # Keep that pin matching MQSSSubmitter's own, so a unit-test binary runs
  # against the same QDMI revision whichever fetch provided the driver.
  FetchContent_Declare(
    qdmi
    GIT_REPOSITORY ${QDMI_REPOSITORY}
    GIT_TAG ${QRMCI_QDMI_GIT_TAG}
    SYSTEM)
  FetchContent_MakeAvailable(qdmi)

  if(_qrmci_qdmi_had_shared_libs)
    set(BUILD_SHARED_LIBS
        ${_qrmci_qdmi_saved_shared_libs}
        CACHE BOOL "" FORCE)
  else()
    unset(BUILD_SHARED_LIBS CACHE)
  endif()
endif()
