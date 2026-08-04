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
  ca-certificates && \
  ln -sf /usr/bin/python3 /usr/bin/python && \
  [ -e /usr/bin/pip ] || ln -s /usr/bin/pip3 /usr/bin/pip && \
  apt-get clean && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://pyenv.run | bash
# Upgrade pip, setuptools, wheel
# RUN python3 -m pip install --upgrade pip setuptools wheel

# Install Python 3.11.14 using pyenv required for MQSSQCS
RUN pyenv install -s "3.11.14"

# Install CUDAQ (allowing pre-releases)
#RUN python3 -m pip install --pre cudaq

#RUN python3 -m pip install numpy uvicorn llvmlite

# Set working directory
WORKDIR /workspace

#RUN bash scripts/install-zlib.sh
