# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

variable "DOCKERFILE_DISTRIBUTED" {
  default = "Dockerfile"
}

variable "IMAGE_NAME_DISTRIBUTED_SELECTOR" {
  default = "qrmci-distributed-selector"
}

variable "IMAGE_NAME_DISTRIBUTED_WORKER" {
  default = "qrmci-distributed-worker"
}

variable "IMAGE_TAG_DISTRIBUTED" {
  default = "latest"
}

# Optional: a GitHub token (e.g. a fine-grained PAT with read access to the
# Munich-Quantum-Software-Stack repositories) used to authenticate the builder
# stage's FetchContent git clones over HTTPS. Only mounted as a secret when
# set; without it the clones stay unauthenticated.
variable "GITHUB_TOKEN" {
  default = ""
}

group "default" {
  targets = ["selector", "worker"]
}

target "_common" {
  context    = "."
  dockerfile = "${DOCKERFILE_DISTRIBUTED}"
  secret     = GITHUB_TOKEN != "" ? ["id=github_token,env=GITHUB_TOKEN"] : []
}

target "selector" {
  inherits = ["_common"]
  target   = "runtime-selector"
  tags     = ["${IMAGE_NAME_DISTRIBUTED_SELECTOR}:${IMAGE_TAG_DISTRIBUTED}"]
}

target "worker" {
  inherits = ["_common"]
  target   = "runtime-worker"
  tags     = ["${IMAGE_NAME_DISTRIBUTED_WORKER}:${IMAGE_TAG_DISTRIBUTED}"]
}
