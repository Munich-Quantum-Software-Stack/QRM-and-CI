IMAGE_NAME ?= qrmci-daemon
IMAGE_TAG ?= 0.1.0
DOCKERFILE ?= docker/Dockerfile

.PHONY: docker-build docker-run docker-clean

docker-build:
	docker build --ssh default=${HOME}/.ssh/lrz-cc -f $(DOCKERFILE) -t $(IMAGE_NAME):$(IMAGE_TAG) .

docker-run:
	docker run --rm -it $(IMAGE_NAME):$(IMAGE_TAG)

docker-clean:
	docker rmi $(IMAGE_NAME):$(IMAGE_TAG) 2>/dev/null || true
