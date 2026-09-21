#!/usr/bin/env bash
# Creates the QRM&CI dev container from qrmci-dev-image, mounting the repo
# root (host) at /workspace (container).
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

IMAGE_NAME="qrmci-dev-image"
IMAGE_TAG="${1:-latest}"
CONTAINER_NAME="qrmci-dev-container"

if docker ps -a --format '{{.Names}}' | grep -qx "${CONTAINER_NAME}"; then
  echo "Container '${CONTAINER_NAME}' already exists. Remove it first with:"
  echo "  docker rm -f ${CONTAINER_NAME}"
  exit 1
fi

# Forward the host ssh-agent so `cmake` can fetch private MQSS repos
# (e.g. MQSSIntegrationDeploymentFramework, PyQDMI) from inside the
# container using your own key.
SSH_MOUNT_ARGS=()
case "$(uname -s)" in
  Darwin)
    # Docker Desktop for Mac runs containers inside a hidden Linux VM, so
    # the host's real ssh-agent socket (e.g. $SSH_AUTH_SOCK on macOS) isn't
    # visible to it and can't be tested for with `[ -S ... ]` here. Docker
    # Desktop instead runs a relay to the host agent and exposes it at a
    # fixed path *inside that VM*: /run/host-services/ssh-auth.sock. That
    # path only exists from the VM/container's point of view, never on the
    # actual macOS filesystem, so we mount it unconditionally and rely on
    # Docker Desktop (ssh-agent forwarding is on by default) to back it.
    SSH_MOUNT_ARGS=(
      --mount type=bind,source=/run/host-services/ssh-auth.sock,target=/ssh-agent
      -e SSH_AUTH_SOCK=/ssh-agent
    )
    ;;
  Linux)
    if [ -n "${SSH_AUTH_SOCK:-}" ] && [ -S "${SSH_AUTH_SOCK}" ]; then
      SSH_MOUNT_ARGS=(
        --mount type=bind,source="${SSH_AUTH_SOCK}",target=/ssh-agent
        -e SSH_AUTH_SOCK=/ssh-agent
      )
    fi
    ;;
esac

if [ "${#SSH_MOUNT_ARGS[@]}" -eq 0 ]; then
  echo "Warning: no host ssh-agent detected, private git fetches inside the container may fail." >&2
fi

docker create \
  --name "${CONTAINER_NAME}" \
  --platform linux/amd64 \
  -it \
  --mount type=bind,source="${PROJECT_ROOT}",target=/workspace \
  "${SSH_MOUNT_ARGS[@]}" \
  -w /workspace \
  "${IMAGE_NAME}:${IMAGE_TAG}" \
  bash -c '[ -S "$SSH_AUTH_SOCK" ] && sudo chmod 666 "$SSH_AUTH_SOCK"; exec bash'

echo "Created container '${CONTAINER_NAME}'."
echo "Start it with:   docker start -ai ${CONTAINER_NAME}"
echo "Reattach with:   docker start -ai ${CONTAINER_NAME}"
echo "Exec into it:    docker exec -it ${CONTAINER_NAME} bash"
