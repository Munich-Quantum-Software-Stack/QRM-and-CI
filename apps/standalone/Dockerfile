# syntax=docker/dockerfile:1.7
# ==========================================
# STAGE 1: Builder
# ==========================================
FROM ubuntu:26.10 AS builder

USER root
ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /workspace

# --- Build toolchain & dev headers -------------------------------------
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    cmake \
    ninja-build \
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

# SSH setup for cloning private repos
RUN mkdir -p /root/.ssh && \
    ssh-keyscan github.com > /root/.ssh/known_hosts

ENV CCACHE_DIR=/root/.cache/ccache
# --- QRMCI and dependencies build ------------------------------------------------
RUN --mount=type=ssh \
    --mount=type=bind,source=.,target=/workspace/src,rw=false \
    --mount=type=cache,target=/root/.cache/ccache,sharing=locked \
    cmake -S /workspace/src -B /workspace/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_QDMI_EXAMPLES=ON \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    && cmake --build /workspace/build --target qrmcid-standalone    --parallel $(nproc) \
    && cmake --build /workspace/build --target mqss-opt            --parallel $(nproc) \
    && cmake --build /workspace/build --target qdmi_example_driver --parallel $(nproc) \
    && cmake --build /workspace/build --target cxx-qdmi-device     --parallel $(nproc)

# ==========================================
# STAGE 2: Runtime
# ==========================================
FROM ubuntu:26.10 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# --- Runtime-only dependencies -------------------------------------------
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
    libboost-chrono1.90.0 \
    libprotobuf32t64 \
    librabbitmq4 \
    libspdlog1.15 \
    libfmt10 \
    libssl4 \
    libzstd1 \
    libc6 \
    libgcc-s1 \
    libstdc++6 \
    zlib1g \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG USERNAME=qrmci
ARG USER_UID=1001
ARG USER_GID=$USER_UID

ENV HOME=/home/${USERNAME} \
    QRM_LOG_DIR=/home/${USERNAME}/logs \
    QDMI_DRIVER_LIB_NAME=qdmi_example_driver \
    QDMI_CONF=/home/${USERNAME}/qdmi.conf \
    LD_LIBRARY_PATH=/usr/local/lib/qrmci:/usr/local/lib \
    PATH=/usr/local/bin:${PATH}

# Create unprivileged user & directories
RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} -s /bin/bash \
    && mkdir -p ${HOME}/logs /usr/local/lib/qrmci \
    && chown -R ${USER_UID}:${USER_GID} ${HOME} /usr/local/lib/qrmci

# --- Copy build artifacts -------------------------------------------------
COPY --from=builder --chown=${USER_UID}:${USER_GID} \
    /workspace/build/apps/standalone/qrmcid-standalone \
    /workspace/build/_deps/mqssci-build/mqss-opt \
    /usr/local/bin/

COPY --from=builder --chown=${USER_UID}:${USER_GID} \
    /workspace/build/libqrmci* \
    /workspace/build/_deps/mqsssubmitter-build/libmqss_submitter.so \
    /workspace/build/_deps/pyqdmi-build/src/cpp/libqdmi_cpp.so \
    /workspace/build/_deps/qdmi-build/examples/driver/libqdmi_example_driver.so \
    /workspace/build/_deps/qdmi-build/examples/device/src/libcxx-qdmi-device.so \
    /workspace/build/_deps/mqssci-build/lib/MQSSCIInterfaces/libMQSS* \
    /workspace/build/_deps/mqssci-build/lib/Passes/Transforms/libMQSS* \
    /workspace/build/_deps/mqssintegrationdeploymentframework-build/libmqss_*.so \
    /workspace/build/_deps/simple_amqp_client-build/libSimple* \
    /usr/local/lib/qrmci/

# Switch to non-root user
USER ${USERNAME}
WORKDIR ${HOME}

RUN echo "/usr/local/lib/qrmci/libcxx-qdmi-device.so CXX" > ${HOME}/qdmi.conf

ENTRYPOINT ["/usr/local/bin/qrmcid-standalone"]
