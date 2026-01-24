#!/bin/bash

# gRPC Installation Script for DANN
# This script downloads and builds gRPC and protobuf for the DANN project

set -e

INSTALL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/grpc"
INSTALL_PREFIX="${INSTALL_DIR}/install"
BUILD_DIR="${INSTALL_DIR}/build"

# Version configuration
GRPC_VERSION="1.54.3"
PROTOBUF_VERSION="23.4"

# Detect operating system
OS="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
fi

# Get CPU count for parallel builds
if command -v nproc &> /dev/null; then
    CPU_COUNT=$(nproc)
elif command -v sysctl &> /dev/null; then
    CPU_COUNT=$(sysctl -n hw.ncpu)
else
    CPU_COUNT=4
fi

echo "Installing gRPC ${GRPC_VERSION} and protobuf ${PROTOBUF_VERSION}..."
echo "OS: $OS, CPU Count: $CPU_COUNT"

# Create directories
mkdir -p "${BUILD_DIR}"
mkdir -p "${INSTALL_PREFIX}"

# Check if already installed
if [ -f "${INSTALL_PREFIX}/lib/libgrpc++.a" ] && [ -f "${INSTALL_PREFIX}/lib/libprotobuf.a" ]; then
    echo "gRPC and protobuf already installed at ${INSTALL_PREFIX}"
    echo "To reinstall, remove the directory first: rm -rf ${INSTALL_DIR}"
    exit 0
fi

# Check system dependencies
echo "Checking system dependencies..."

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is required but not installed."
    if [ "$OS" = "macos" ]; then
        echo "Install with: brew install cmake"
    else
        echo "Install with: apt-get install cmake (Ubuntu/Debian) or yum install cmake (CentOS/RHEL)"
    fi
    exit 1
fi

# Check for git
if ! command -v git &> /dev/null; then
    echo "Error: git is required but not installed."
    if [ "$OS" = "macos" ]; then
        echo "Install with: brew install git"
    else
        echo "Install with: apt-get install git (Ubuntu/Debian) or yum install git (CentOS/RHEL)"
    fi
    exit 1
fi

# macOS-specific checks
if [ "$OS" = "macos" ]; then
    # Check for Xcode Command Line Tools
    if ! xcode-select -p &> /dev/null; then
        echo "Error: Xcode Command Line Tools are required but not installed."
        echo "Install with: xcode-select --install"
        exit 1
    fi
    
    # Check for Homebrew (optional but recommended)
    if command -v brew &> /dev/null; then
        echo "✓ Homebrew found"
    else
        echo "Warning: Homebrew not found. Some dependencies might need manual installation."
    fi
fi

# Download and build protobuf first
echo "Building protobuf ${PROTOBUF_VERSION}..."
cd "${BUILD_DIR}"

if [ ! -d "protobuf-${PROTOBUF_VERSION}" ]; then
    echo "Downloading protobuf ${PROTOBUF_VERSION}..."
    curl -L "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-${PROTOBUF_VERSION}.tar.gz" | tar xz
fi

cd "protobuf-${PROTOBUF_VERSION}"
mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -Dprotobuf_BUILD_TESTS=OFF \
      -Dprotobuf_BUILD_EXAMPLES=OFF \
      ..

make -j${CPU_COUNT}
make install

# Download and build gRPC
echo "Building gRPC ${GRPC_VERSION}..."
cd "${BUILD_DIR}"

if [ ! -d "grpc-${GRPC_VERSION}" ]; then
    echo "Downloading gRPC ${GRPC_VERSION}..."
    curl -L "https://github.com/grpc/grpc/archive/v${GRPC_VERSION}.tar.gz" | tar xz
fi

cd "grpc-${GRPC_VERSION}"

# Update git submodules
git submodule update --init --recursive

mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_BUILD_EXAMPLES=OFF \
      -DgRPC_BUILD_CSHARP_EXT=OFF \
      -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
      -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
      -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
      -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
      -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF \
      -DgRPC_INSTALL=ON \
      -DgRPC_ABSL_PROVIDER=module \
      -DgRPC_PROTOBUF_PROVIDER=package \
      -DProtobuf_DIR="${INSTALL_PREFIX}/lib/cmake/protobuf" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
      ..

make -j${CPU_COUNT}
make install

# Verify installation
echo "Verifying installation..."
if [ -f "${INSTALL_PREFIX}/lib/libgrpc++.a" ] && [ -f "${INSTALL_PREFIX}/lib/libprotobuf.a" ]; then
    echo "✓ gRPC and protobuf successfully installed!"
    echo ""
    echo "Installation summary:"
    echo "  gRPC version: ${GRPC_VERSION}"
    echo "  protobuf version: ${PROTOBUF_VERSION}"
    echo "  Install prefix: ${INSTALL_PREFIX}"
    echo ""
    echo "Libraries installed:"
    ls -la "${INSTALL_PREFIX}/lib/" | grep -E "(libgrpc|libprotobuf)"
    echo ""
    echo "Headers installed in: ${INSTALL_PREFIX}/include"
    echo "Tools installed in: ${INSTALL_PREFIX}/bin"
    echo ""
    echo "You can now build DANN with gRPC support."
else
    echo "✗ Installation failed. Please check the error messages above."
    exit 1
fi

# Create a version file for reference
echo "${GRPC_VERSION}" > "${INSTALL_PREFIX}/.grpc_version"
echo "${PROTOBUF_VERSION}" > "${INSTALL_PREFIX}/.protobuf_version"

echo "Installation completed successfully!"
