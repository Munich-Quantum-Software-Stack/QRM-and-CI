#!/usr/bin/env bash
# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"

# Fails loudly, before any build or daemon spawn, if a required credential
# wasn't exported by the caller. These must be `export`ed (or passed as
# `VAR=value ./run.sh`) in the invoking shell -- this script only checks
# for them, it never prompts or supplies its own values, since a real
# LRZ backend should never silently fall back to a placeholder credential.
require_env() {
  local var_name="$1"
  if [[ -z "${!var_name:-}" ]]; then
    echo "error: ${var_name} must be set -- see tests/real-backend/real-backend.md" >&2
    exit 1
  fi
}

require_env QRMCI_AMQP_HOST
require_env QRMCI_AMQP_USER
require_env QRMCI_AMQP_PASSWORD
require_env QRMCI_SUBMITTER_QDMI_CLIENT_TOKEN
require_env QRMCI_SUBMITTER_QDMI_DEVICE_NAME   # e.g. LRZ
require_env LRZ_QDMI_DEVICE_QPU               # e.g. EQE1

cmake --build "${BUILD_DIR}" --target TestLRZWorker

# TestLRZWorker's own loadConfig() call reads this directly from the
# environment (src/Config.cpp) -- it has no compile-time equivalent, unlike
# the daemon-facing config paths baked in via tests/real-backend/CMakeLists.txt.
export QRMCI_CONFIG_FILE="${SCRIPT_DIR}/config/selector-test-config.toml"

exec "${BUILD_DIR}/tests/real-backend/LRZ/TestLRZWorker"
