#!/bin/bash
set -e  # Exit immediately on error


CURRENT_DIR=$(pwd)

INSTALL_PATH="${INSTALL_PATH:-$HOME}"
# Default values
NUM_JOBS=4  # Default number of jobs
BUILD_DOCS=OFF  # Default: Do not build documentation
BUILD_TESTS=OFF  # Default: Do not build tests
BUILD_TYPE="Release"  # Default: Release mode

# # Default directories (can be overridden by arguments)
# MLIR_DIR="/usr/local/llvm/lib/cmake/mlir"
# CLANG_DIR="/usr/local/llvm/lib/cmake/clang"
# LLVM_DIR="/usr/local/llvm/lib/cmake/llvm"

# Parse command-line arguments

BUILD_DIR=${CURRENT_DIR}"/build"
DEPS_DIR="${BUILD_DIR}/_deps"
LOGS_DIR="${BUILD_DIR}/logs"

mkdir -p ${BUILD_DIR}
mkdir -p ${DEPS_DIR}
mkdir -p ${LOGS_DIR}

echo ${BUILD_DIR}
cd  "${BUILD_DIR}" || { echo "Failed to navigate back to the original directory."; exit 1; }

echo "Configuring the QRM repository CMake..."
cmake .. \
  -DCMAKE_C_COMPILER=gcc-13 \
  -DCMAKE_CXX_COMPILER=g++-13 \
  -DCMAKE_INSTALL_PREFIX=${INSTALL_PATH} \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

if [ $? -ne 0 ]; then
  echo "CMake configuration failed."
  exit 1
fi

echo "Building QRM with ${NUM_JOBS} jobs..."
# ninja -j "${NUM_JOBS}" -C ${BUILD_DIR}
make -j ${NUM_JOBS} -C ${BUILD_DIR}
#make install
echo "Build of QRM completed successfully!..."
