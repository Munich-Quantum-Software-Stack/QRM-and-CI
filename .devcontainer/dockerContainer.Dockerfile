# [Operating System]
ARG base_image=ubuntu:22.04

# [CUDA-Q Dependencies]
FROM ${base_image} AS prereqs
SHELL ["/bin/bash", "-c"]
ARG toolchain=gcc11

# When a dialogue box would be needed during install, assume default configurations.
# Set here to avoid setting it for all install commands.
# Given as arg to make sure that this value is only set during build but not in the launched container.
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && \
    apt-get autoremove -y --purge && apt-get clean && rm -rf /var/lib/apt/lists/*

## [Prerequisites]
RUN apt-get update && apt-get install -y --no-install-recommends python3 && \
    apt-get autoremove -y --purge && apt-get clean && rm -rf /var/lib/apt/lists/*

## [Build Dependencies]
RUN apt-get update && apt-get install -y --no-install-recommends \
        wget git unzip \
        python3-dev python3-pip && \
    python3 -m pip install --no-cache-dir numpy && \
    apt-get autoremove -y --purge && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
    build-essential \
    libz3-dev \
    openssh-client \
    libgtest-dev \
    pkg-config \
    bison \
    flex \
    libeigen3-dev \
    libboost-program-options-dev \
    libzip-dev && \
    # Clean up cache to reduce image size
    rm -rf /var/lib/apt/lists/*

# Clone LLVM
WORKDIR /opt
# RUN git clone --depth 1 https://github.com/llvm/llvm-project.git
RUN git clone --branch llvmorg-16.0.6 --depth 1 https://github.com/llvm/llvm-project.git

# Configure
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
  -DLLVM_BUILD_EXAMPLES=OFF \
  -DLLVM_INSTALL_UTILS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_INSTALL_PREFIX=/opt/llvm

# Build and install
RUN ninja -j6 install

# Set working directory for dev
WORKDIR /workspace

COPY --from=builder /opt/llvm /usr/local/llvm
ENV PATH=/usr/local/llvm/bin:$PATH
