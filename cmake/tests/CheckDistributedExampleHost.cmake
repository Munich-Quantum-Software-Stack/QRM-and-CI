# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Regression check: apps/distributed/.env.example must point at the shared
# external broker via the Docker host alias, not at each container's own
# loopback. Invoked as a CTest test (see CMakeLists.txt).
#
# cmake -DENV_EXAMPLE_FILE=<path> -P
# cmake/tests/CheckDistributedExampleHost.cmake

if(NOT DEFINED ENV_EXAMPLE_FILE)
  message(
    FATAL_ERROR "ENV_EXAMPLE_FILE must be set to apps/distributed/.env.example")
endif()

if(NOT EXISTS "${ENV_EXAMPLE_FILE}")
  message(FATAL_ERROR "${ENV_EXAMPLE_FILE} does not exist")
endif()

file(STRINGS "${ENV_EXAMPLE_FILE}" lines)
list(FIND lines "QRMCI_AMQP_HOST=localhost" expected_line_index)
if(expected_line_index EQUAL -1)
  message(
    FATAL_ERROR
      "${ENV_EXAMPLE_FILE} must contain exactly 'QRMCI_AMQP_HOST=localhost'")
endif()
