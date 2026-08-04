FROM ubuntu:26.10 AS builder

USER root

ENV DEBIAN_FRONTEND=noninteractive \
    CMAKE_BUILD_PARALLEL_LEVEL=4 \
    QRM_LOG_DIR=/workspace/logs

WORKDIR /workspace

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
    librabbitmq-dev libspdlog-dev nlohmann-json3-dev
RUN apt-get update && apt-get install -y --no-install-recommends \
    libzstd-dev libprotobuf-dev protobuf-compiler
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-dev python3-pip python-is-python3
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-dev libboost-chrono-dev
RUN apt-get update && apt-get install -y --no-install-recommends \
    doxygen graphviz libgtest-dev
RUN apt-get update && apt-get install -y --no-install-recommends \
    libspdlog-dev libfmt-dev openssh-client wget
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++

RUN curl -fsSL https://pyenv.run | bash

RUN wget https://github.com/NVIDIA/cuda-quantum/releases/download/0.15.0/install_cuda_quantum_cu13.x86_64 \
    && chmod +x install_cuda_quantum_cu13.x86_64 \
    && ./install_cuda_quantum_cu13.x86_64 --accept \
    && . /etc/profile
SHELL ["/bin/bash", "-c"]
RUN source /opt/nvidia/cudaq/set_env.sh

COPY . /workspace

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    pybind11-dev \
    libboost-graph-dev \
    libz3-dev


RUN mkdir -p /root/.ssh && \
    ssh-keyscan github.com > /root/.ssh/known_hosts

RUN --mount=type=ssh \
    cmake -S /workspace -B /workspace/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_QDMI_EXAMPLES=ON \
    && sed -i '/#include <variant>/a #include <cstdint>' \
    build/_deps/pyqdmi-src/include/qdmi.hpp \
    && sed -i '/#include <vector>/a #include <algorithm>' \
    build/_deps/pyqdmi-src/src/cpp/qdmi_device.cpp \
    && cmake --build /workspace/build --target qrmci_aio_daemon -j"$(nproc)" \
    && cmake --build /workspace/build --target qdmi_example_driver -j"$(nproc)" \
    && cmake --build /workspace/build --target cxx_device -j"$(nproc)"

RUN cd /workspace/build/_deps/mqssci-src && \
    ls -l /workspace/build/_deps/mqssci-build && \
    make build && make target && \
    ls -l build/bin

####---------------------------------------------------------------------###

FROM ubuntu:26.10 AS runtime

ARG USERNAME=qrmci
ARG USER_UID=1001
ARG USER_GID=$USER_UID
RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} -s /bin/bash \
    && chown -R $USER_UID:$USER_GID /home/$USERNAME \
    && apt-get update \
    && apt-get install -y sudo \
    && echo ${USERNAME} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}
ENV HOME=/home/qrmci

RUN mkdir -p ${HOME} && \
    chown -R ${USER_UID}:${USER_GID} ${HOME} && \
    chmod -R 755 /home/${USERNAME}
WORKDIR ${HOME}

USER ${USERNAME}

ENV DEBIAN_FRONTEND=noninteractive \
    QRM_LOG_DIR=${HOME}/logs
RUN mkdir -p ${HOME}/logs
ENV QDMI_DRIVER_LIB_NAME=qdmi_example_driver
ENV QDMI_CONF=${HOME}/qdmi.conf
ENV LD_LIBRARY_PATH=/usr/local/lib
ENV LD_LIBRARY_PATH=/usr/local/lib/qrmci:${LD_LIBRARY_PATH}
ENV LD_LIBRARY_PATH=/opt/nvidia/cudaq/lib:${LD_LIBRARY_PATH}
ENV PATH=/usr/local/bin:${PATH}
ENV PATH=/opt/nvidia/cudaq/bin:${PATH}

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
    libprotobuf-dev \
    librabbitmq-dev \
    libboost-dev \
    libboost-chrono-dev \
    libspdlog-dev \
    libfmt-dev \
    wget \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

SHELL ["/bin/bash", "-c"]

COPY --from=builder /workspace/build/deployment/workflow-aio/qrmci_aio_daemon /usr/local/bin/qrmci_aio_daemon
COPY --from=builder /workspace/build/_deps/mqssci-src/build/bin/mqss-opt /usr/local/bin/

COPY --from=builder /workspace/build/libqrmci.so /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/mqsscompiler-build/libmqss_compiler.so /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/mqssci-src/build/lib/Passes/libMQSS* /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/mqsssubmitter-build/libmqss_submitter.so /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/mqssintegrationdeploymentframework-build/libmqss_*.so /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/pyqdmi-build/src/cpp/libqdmi_cpp.so /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/simple_amqp_client-build/libSimple* /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/qdmi-build/examples/driver/libqdmi_example_driver.so /usr/local/lib/qrmci/
COPY --from=builder /workspace/build/_deps/qdmi-build/examples/device/libcxx_device.so /usr/local/lib/qrmci/

USER ${USERNAME}
RUN echo "/usr/local/lib/qrmci/libcxx_device.so CXX" > ${HOME}/qdmi.conf
ENTRYPOINT ["/usr/local/bin/qrmci_aio_daemon"]
