ARG DEPS_IMAGE=ghcr.io/akshay9594/mqss-passes-suite/mqssci-deps:llvm22.1.0-cudaq0.15.0-catalyst0.15.0
FROM ${DEPS_IMAGE}-${TARGETARCH} AS deps

FROM ubuntu:26.10

SHELL ["/bin/bash", "-c"]
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin

ENV CCACHE_DIR=/root/.cache/ccache

# Set working directory
WORKDIR /workspace

# Install build dependencies
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
  apt-get update && apt-get install -y --no-install-recommends \
  build-essential \
  g++ \
  cmake \
  ninja-build \
  pre-commit \
  ccache \
  git \
  pkg-config \
  ca-certificates \
  curl \
  gnupg \
  lsb-release \
  openssh-client \
  librabbitmq-dev \
  libspdlog-dev \
  nlohmann-json3-dev \
  libfmt-dev \
  libzstd-dev \
  libprotobuf-dev \
  protobuf-compiler \
  python3-dev \
  python3-pip \
  python-is-python3 \
  pybind11-dev \
  libboost-dev \
  libboost-chrono-dev \
  libboost-graph-dev \
  libz3-dev \
  doxygen \
  graphviz \
  libgtest-dev \
  && rm -rf /var/lib/apt/lists/*

# --- External toolchains (Needed for MQSSCI) ------------
COPY --from=deps /opt/deps /opt/deps
ENV LLVM_DIR=/opt/deps/llvm/lib/cmake/llvm \
  MLIR_DIR=/opt/deps/llvm/lib/cmake/mlir \
  CUDAQ_INSTALL_PREFIX=/opt/deps/cudaq \
  CATALYST_INSTALL_PREFIX=/opt/deps/catalyst
