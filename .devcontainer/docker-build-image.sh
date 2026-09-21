#!/usr/bin/env bash
# Builds the QRM&CI dev image from .devcontainer/Dockerfile.
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

IMAGE_NAME="qrmci-dev-image"
IMAGE_TAG="${1:-latest}"

# CUDA-Quantum (installed later on top of this image) only ships x86_64
# builds, so pin amd64 explicitly rather than let Docker silently pick the
# host's native arch (arm64 on Apple Silicon) and fail later mid-install.
docker build \
  --platform linux/amd64 \
  -f "${SCRIPT_DIR}/Dockerfile" \
  -t "${IMAGE_NAME}:${IMAGE_TAG}" \
  "${SCRIPT_DIR}"

echo "Built ${IMAGE_NAME}:${IMAGE_TAG}"
