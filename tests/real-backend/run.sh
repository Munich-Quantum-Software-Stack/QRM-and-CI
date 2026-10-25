#!/usr/bin/env bash
# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"

cmake --build "${BUILD_DIR}" --target TestIQMWorker

# TestIQMWorker's own loadConfig() call reads this directly from the
# environment (src/Config.cpp) -- it has no compile-time equivalent, unlike
# the daemon-facing config paths baked in via tests/real-backend/CMakeLists.txt.
export QRMCI_CONFIG_FILE="${SCRIPT_DIR}/config/selector-test-config.toml"

exec "${BUILD_DIR}/tests/real-backend/TestIQMWorker"
