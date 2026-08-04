# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Dynamic defaults generated via cmake build-system context

# Logging
set(QRMCI_LOG_DIR
    "${CMAKE_BINARY_DIR}/logs"
    CACHE PATH "Default log directory")

# Benchmarks
set(QRMCI_BENCHMARK_DIR
    "${CMAKE_SOURCE_DIR}/benchmarks"
    CACHE PATH "Benchmark directory")
