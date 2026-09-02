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

variable "SSH_KEY_PATH" {
  default = ""
}

group "default" {
  targets = ["selector", "worker"]
}

target "_common" {
  context    = "."
  dockerfile = "${DOCKERFILE_DISTRIBUTED}"
  ssh        = ["default=${SSH_KEY_PATH}"]
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
