IMAGE_NAME ?= qrmcid-standalone
IMAGE_TAG ?= 0.1.0
DOCKERFILE ?= apps/standalone/Dockerfile

.PHONY: docker-build docker-run docker-clean

docker-build:
	docker build --ssh default=${HOME}/.ssh/lrz-cc -f $(DOCKERFILE) -t $(IMAGE_NAME):$(IMAGE_TAG) .

docker-build-builder:
	docker build --ssh default=${HOME}/.ssh/lrz-cc -f $(DOCKERFILE) --target builder -t $(IMAGE_NAME)-builder:$(IMAGE_TAG) .

docker-run:
	docker run --rm -it $(IMAGE_NAME):$(IMAGE_TAG)

docker-clean:
	docker rmi $(IMAGE_NAME):$(IMAGE_TAG) 2>/dev/null || true
