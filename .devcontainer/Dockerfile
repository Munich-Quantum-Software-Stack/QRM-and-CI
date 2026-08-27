# ubuntu:26.10 ships GCC 15, which is stricter about transitive standard
# library includes than PyQDMI's pinned qdmi.hpp/qdmi_device.cpp expect.
# After `cmake -S . -B build`, qdmi.hpp is missing <cstdint> (uint64_t) and
# qdmi_device.cpp is missing <algorithm> (std::find), so building fails
# until those two headers are patched in (see docker/Dockerfile's sed
# commands for the exact fix, run before `cmake --build`).
ARG base_image=ubuntu:26.10
FROM ${base_image}
SHELL ["/bin/bash", "-c"]
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential doxygen graphviz pre-commit\
  wget git unzip librabbitmq-dev\
  pyenv libzstd-dev libprotobuf-dev protobuf-compiler libboost-all-dev\
  python3 python3-dev python3-pip \
  cmake ninja-build \
  libz3-dev openssh-client libgtest-dev pkg-config \
  bison flex libeigen3-dev libboost-program-options-dev libzip-dev \
  autoconf automake libtool libspdlog-dev libopenblas-dev \
  nlohmann-json3-dev uuid-dev rabbitmq-server libssl-dev  \
  ca-certificates sudo && \
  ln -sf /usr/bin/python3 /usr/bin/python && \
  [ -e /usr/bin/pip ] || ln -s /usr/bin/pip3 /usr/bin/pip && \
  apt-get clean && rm -rf /var/lib/apt/lists/*

# Create a non-root admin user with passwordless sudo
ARG USERNAME=qrmci
ARG USER_UID=1001
ARG USER_GID=$USER_UID
RUN groupadd --gid ${USER_GID} ${USERNAME} \
  && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} -s /bin/bash \
  && echo ${USERNAME} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${USERNAME} \
  && chmod 0440 /etc/sudoers.d/${USERNAME}

RUN curl -fsSL https://pyenv.run | bash
# Upgrade pip, setuptools, wheel
# RUN python3 -m pip install --upgrade pip setuptools wheel

# Install Python 3.11.14 using pyenv required for MQSSQCS
RUN pyenv install -s "3.11.14"

# Install CUDAQ (allowing pre-releases)
#RUN python3 -m pip install --pre cudaq

#RUN python3 -m pip install numpy uvicorn llvmlite

# Set working directory
RUN mkdir -p /workspace && chown -R ${USER_UID}:${USER_GID} /workspace
WORKDIR /workspace

#RUN bash scripts/install-zlib.sh

USER ${USERNAME}
