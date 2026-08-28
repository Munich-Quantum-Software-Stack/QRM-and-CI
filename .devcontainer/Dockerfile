ARG base_image=ubuntu:26.10
FROM ${base_image}
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
RUN curl -fsSL https://pyenv.run | bash
