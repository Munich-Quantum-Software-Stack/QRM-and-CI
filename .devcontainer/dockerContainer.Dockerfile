ARG base_image=ubuntu:22.04

FROM ${base_image} AS builder
SHELL ["/bin/bash", "-c"]
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    wget git unzip \
    python3 python3-dev python3-pip \
    cmake ninja-build \
    libz3-dev openssh-client libgtest-dev pkg-config \
    bison flex libeigen3-dev libboost-program-options-dev libzip-dev \
    ca-certificates && \
    ln -s /usr/bin/python3 /usr/bin/python && \
    ln -s /usr/bin/pip3 /usr/bin/pip && \
    python -m pip install --no-cache-dir numpy && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Clone LLVM
WORKDIR /opt
RUN git clone --branch llvmorg-16.0.6 --depth 1 https://github.com/llvm/llvm-project.git

# Configure LLVM
WORKDIR /opt/llvm-project/build
RUN cmake -G Ninja ../llvm \
  -DLLVM_ENABLE_PROJECTS="mlir;clang" \
  -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  -DLLVM_PARALLEL_COMPILE_JOBS=6 \
  -DLLVM_PARALLEL_LINK_JOBS=6 \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_ENABLE_RTTI=ON \
  -DLLVM_ENABLE_EH=ON \
  -DLLVM_INSTALL_UTILS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_INSTALL_PREFIX=/opt/llvm

RUN ninja -j6 install

# Final image
FROM ${base_image} AS final
COPY --from=builder /opt/llvm /usr/local/llvm
ENV PATH=/usr/local/llvm/bin:$PATH
WORKDIR /workspace
