# syntax=docker/dockerfile:1.7
# ==========================================
# STAGE 1: Builder (shared by both runtime images)
# ==========================================
ARG DEPS_IMAGE=ghcr.io/akshay9594/mqss-passes-suite/mqssci-deps:llvm22.1.0-cudaq0.15.0-catalyst0.15.0
FROM ${DEPS_IMAGE}-${TARGETARCH} AS deps

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

ENV CCACHE_DIR=/root/.cache/ccache
# --- QRM&CI and dependencies build -----------------------------------------
# github_token is an optional BuildKit secret (see Makefile/docker-bake.hcl): when
# provided, it authenticates the FetchContent git clones (e.g. MQSS-Submitter) over
# HTTPS via a per-process git config override -- never written to disk, so it
# never lands in an image layer. Without the secret the clones fall back to
# unauthenticated HTTPS, which only works while those repositories are public.
RUN --mount=type=secret,id=github_token \
    --mount=type=bind,source=.,target=/workspace/src,rw=false \
    --mount=type=cache,target=/root/.cache/ccache,sharing=locked \
    if [ -s /run/secrets/github_token ]; then \
    export GIT_CONFIG_COUNT=1 \
    GIT_CONFIG_KEY_0="http.https://github.com/.extraheader" \
    GIT_CONFIG_VALUE_0="AUTHORIZATION: basic $(printf 'x-access-token:%s' "$(cat /run/secrets/github_token)" | base64 -w0)"; \
    fi && \
    cmake -S /workspace/src -B /workspace/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_QDMI_EXAMPLES=ON \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    && cmake --build /workspace/build --target qrmcid-distributed-selector --parallel $(nproc) \
    && cmake --build /workspace/build --target qrmcid-distributed-worker   --parallel $(nproc) \
    && cmake --build /workspace/build --target mqss-opt            --parallel $(nproc) \
    && cmake --build /workspace/build --target qdmi_example_driver --parallel $(nproc) \
    && cmake --build /workspace/build --target cxx-qdmi-device     --parallel $(nproc)

# ==========================================
# STAGE 2a: Selector runtime
# ==========================================
FROM ubuntu:26.10 AS runtime-selector

ENV DEBIAN_FRONTEND=noninteractive

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
    QRMCI_LOG_DIR=/home/${USERNAME}/logs \
    QRMCI_AMQP_HOST=localhost \
    LD_LIBRARY_PATH=/usr/local/lib/qrmci:/usr/local/lib \
    PATH=/usr/local/bin:${PATH}

RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} -s /bin/bash \
    && mkdir -p ${HOME}/logs /usr/local/lib/qrmci \
    && chown -R ${USER_UID}:${USER_GID} ${HOME} /usr/local/lib/qrmci

COPY --from=builder --chown=${USER_UID}:${USER_GID} \
    /workspace/build/apps/distributed/qrmcid-distributed-selector \
    /usr/local/bin/

COPY --from=builder --chown=${USER_UID}:${USER_GID} \
    /workspace/build/libqrmci* \
    /workspace/build/_deps/mqss_submitter-build/src/libmqss_submitter.so \
    /workspace/build/_deps/mqss_scheduler-build/libscheduler.so \
    /workspace/build/_deps/qdmi-build/examples/driver/libqdmi_example_driver.so \
    /workspace/build/_deps/mqssci-build/lib/Dialects/libMQSS* \
    /workspace/build/_deps/mqssci-build/lib/MQSSCIInterfaces/libMQSS* \
    /workspace/build/_deps/mqssci-build/lib/Passes/CodeGen/libMQSS* \
    /workspace/build/_deps/mqssci-build/lib/Passes/Transforms/libMQSS* \
    /workspace/build/_deps/mqt-core-build/src/na/libmqt-core-na* \
    /workspace/build/_deps/mqt-core-build/src/ir/libmqt-core-ir* \
    /workspace/build/_deps/mqt-core-build/src/circuit_optimizer/libmqt-core-circuit-optimizer* \
    /workspace/build/_deps/mqt-qmap-build/src/sc/libmqt-qmap-sc* \
    /workspace/build/_deps/mqt-qmap-build/src/sc/heuristic/libmqt-qmap-sc-heuristic* \
    /workspace/build/_deps/mqssintegrationdeploymentframework-build/libmqss_*.so \
    /workspace/build/_deps/simple_amqp_client-build/libSimple* \
    /usr/local/lib/qrmci/

USER ${USERNAME}
WORKDIR ${HOME}

ENTRYPOINT ["/usr/local/bin/qrmcid-distributed-selector"]

# ==========================================
# STAGE 2b: Worker runtime
# ==========================================
FROM ubuntu:26.10 AS runtime-worker

ENV DEBIAN_FRONTEND=noninteractive

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
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG USERNAME=qrmci
ARG USER_UID=1001
ARG USER_GID=$USER_UID

ENV HOME=/home/${USERNAME} \
    QRMCI_LOG_DIR=/home/${USERNAME}/logs \
    QDMI_DRIVER_LIB_NAME=qdmi_example_driver \
    QDMI_CONF=/home/${USERNAME}/qdmi.conf \
    QRMCI_AMQP_HOST=localhost \
    LD_LIBRARY_PATH=/usr/local/lib/qrmci:/usr/local/lib \
    PATH=/usr/local/bin:${PATH}

RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} -s /bin/bash \
    && mkdir -p ${HOME}/logs /usr/local/lib/qrmci \
    && chown -R ${USER_UID}:${USER_GID} ${HOME} /usr/local/lib/qrmci

COPY --from=builder --chown=${USER_UID}:${USER_GID} \
    /workspace/build/apps/distributed/qrmcid-distributed-worker \
    /workspace/build/_deps/mqssci-build/mqss-opt \
    /usr/local/bin/

COPY --from=builder --chown=${USER_UID}:${USER_GID} \
    /workspace/build/libqrmci* \
    /workspace/build/_deps/mqss_submitter-build/src/libmqss_submitter.so \
    /workspace/build/_deps/mqss_scheduler-build/libscheduler.so \
    /workspace/build/_deps/qdmi-build/examples/driver/libqdmi_example_driver.so \
    /workspace/build/_deps/qdmi-build/examples/device/src/libcxx-qdmi-device.so \
    /workspace/build/_deps/mqssci-build/lib/Dialects/libMQSS* \
    /workspace/build/_deps/mqssci-build/lib/MQSSCIInterfaces/libMQSS* \
    /workspace/build/_deps/mqssci-build/lib/Passes/CodeGen/libMQSS* \
    /workspace/build/_deps/mqssci-build/lib/Passes/Transforms/libMQSS* \
    /workspace/build/_deps/mqt-core-build/src/na/libmqt-core-na* \
    /workspace/build/_deps/mqt-core-build/src/ir/libmqt-core-ir* \
    /workspace/build/_deps/mqt-core-build/src/circuit_optimizer/libmqt-core-circuit-optimizer* \
    /workspace/build/_deps/mqt-qmap-build/src/sc/libmqt-qmap-sc* \
    /workspace/build/_deps/mqt-qmap-build/src/sc/heuristic/libmqt-qmap-sc-heuristic* \
    /workspace/build/_deps/mqssintegrationdeploymentframework-build/libmqss_*.so \
    /workspace/build/_deps/simple_amqp_client-build/libSimple* \
    /usr/local/lib/qrmci/

USER ${USERNAME}
WORKDIR ${HOME}

RUN echo "/usr/local/lib/qrmci/libcxx-qdmi-device.so CXX" > ${HOME}/qdmi.conf

ENTRYPOINT ["/usr/local/bin/qrmcid-distributed-worker"]
