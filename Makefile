# cmake_project_version(path) extracts the `project(... VERSION x.y.z ...)` version from a
# CMakeLists.txt, skipping unrelated VERSION uses (e.g. cmake_minimum_required).
cmake_project_version = $(shell awk '/^[[:space:]]*VERSION[[:space:]]+[0-9]/{print $$2; exit}' $(1))

IMAGE_NAME_STANDALONE ?= qrmcid-standalone
IMAGE_TAG_STANDALONE ?= $(call cmake_project_version,apps/standalone/CMakeLists.txt)
DOCKERFILE_STANDALONE ?= apps/standalone/Dockerfile

DOCKERFILE_DISTRIBUTED ?= apps/distributed/Dockerfile
IMAGE_NAME_DISTRIBUTED_SELECTOR ?= qrmcid-distributed-selector
IMAGE_NAME_DISTRIBUTED_WORKER ?= qrmcid-distributed-worker
IMAGE_TAG_DISTRIBUTED ?= $(call cmake_project_version,apps/distributed/CMakeLists.txt)

SSH_KEY_PATH ?= ${HOME}/.ssh/<ssh-key-name>

# docker-bake.hcl reads these from the environment, so they must be exported
# for the distributed targets.
export DOCKERFILE_DISTRIBUTED IMAGE_NAME_DISTRIBUTED_SELECTOR IMAGE_NAME_DISTRIBUTED_WORKER IMAGE_TAG_DISTRIBUTED SSH_KEY_PATH

.PHONY: docker-build-standalone docker-run-standalone docker-clean-standalone \
	docker-build-distributed docker-build-distributed-selector docker-build-distributed-worker \
	docker-run-distributed-selector docker-run-distributed-worker \
	docker-clean-distributed

docker-build-standalone:
	docker build --ssh default=${SSH_KEY_PATH} -f $(DOCKERFILE_STANDALONE) -t $(IMAGE_NAME_STANDALONE):$(IMAGE_TAG_STANDALONE) .

docker-run-standalone:
	docker run --rm -it $(IMAGE_NAME_STANDALONE):$(IMAGE_TAG_STANDALONE)

docker-clean-standalone:
	docker rmi $(IMAGE_NAME_STANDALONE):$(IMAGE_TAG_STANDALONE) 2>/dev/null || true

docker-build-distributed:
	docker buildx bake --allow=fs.read=$(SSH_KEY_PATH) -f apps/distributed/docker-bake.hcl

docker-build-distributed-selector:
	docker buildx bake --allow=fs.read=$(SSH_KEY_PATH) -f apps/distributed/docker-bake.hcl selector

docker-build-distributed-worker:
	docker buildx bake --allow=fs.read=$(SSH_KEY_PATH) -f apps/distributed/docker-bake.hcl worker

docker-run-distributed-selector:
	docker run --rm -it $(IMAGE_NAME_DISTRIBUTED_SELECTOR):$(IMAGE_TAG_DISTRIBUTED)

docker-run-distributed-worker:
	docker run --rm -it $(IMAGE_NAME_DISTRIBUTED_WORKER):$(IMAGE_TAG_DISTRIBUTED)

docker-clean-distributed:
	docker rmi $(IMAGE_NAME_DISTRIBUTED_SELECTOR):$(IMAGE_TAG_DISTRIBUTED) $(IMAGE_NAME_DISTRIBUTED_WORKER):$(IMAGE_TAG_DISTRIBUTED) 2>/dev/null || true
