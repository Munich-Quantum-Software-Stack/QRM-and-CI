# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Fails if CANARY appears in CMakeCache.txt or the fetched MQSS-Submitter
# .git/config, without ever printing CANARY itself. Run manually after a
# configure that used a non-secret canary in place of a real credential:
#
# cmake -DBUILD_DIR=<build-dir> -DCANARY=<canary> \ -P
# cmake/tests/CheckNoCredentialInFetchMetadata.cmake

if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR must be set to the build directory to inspect")
endif()
if(NOT DEFINED CANARY)
  message(FATAL_ERROR "CANARY must be set to the marker value to search for")
endif()

set(QRMCI_CANARY_CHECK_FILES
    "${BUILD_DIR}/CMakeCache.txt"
    "${BUILD_DIR}/_deps/mqss_submitter-src/.git/config")

foreach(file IN LISTS QRMCI_CANARY_CHECK_FILES)
  if(EXISTS "${file}")
    file(READ "${file}" contents)
    string(FIND "${contents}" "${CANARY}" match_index)
    if(NOT match_index EQUAL -1)
      message(FATAL_ERROR "Credential canary found in ${file}")
    endif()
  endif()
endforeach()

message(STATUS "No credential canary found in inspected fetch metadata")
