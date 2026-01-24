#!/bin/bash

# Fix protobuf conflicts script for DANN
# This script helps resolve protobuf target conflicts when using Homebrew gRPC

set -e

echo "Fixing protobuf conflicts for DANN..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the DANN root directory"
    exit 1
fi

# Clean build directory
if [ -d "build" ]; then
    echo "Cleaning existing build directory..."
    rm -rf build
fi

# Check Homebrew installations
echo "Checking Homebrew installations..."
if command -v brew &> /dev/null; then
    echo "Homebrew found at: $(which brew)"
    
    if brew list grpc &> /dev/null; then
        echo "✓ gRPC is installed via Homebrew"
        echo "gRPC version: $(brew list --versions grpc)"
    else
        echo "✗ gRPC not found via Homebrew"
        echo "Installing gRPC..."
        brew install grpc protobuf
    fi
    
    if brew list protobuf &> /dev/null; then
        echo "✓ protobuf is installed via Homebrew"
        echo "protobuf version: $(brew list --versions protobuf)"
    else
        echo "✗ protobuf not found via Homebrew"
    fi
else
    echo "Homebrew not found. Please install Homebrew first."
    exit 1
fi

# Fix protobuf linking issues
echo "Fixing protobuf linking issues..."

# Reinstall protobuf to fix potential issues
echo "Reinstalling protobuf to fix conflicts..."
brew reinstall protobuf

# Reinstall gRPC
echo "Reinstalling gRPC..."
brew reinstall grpc

# Force link if needed
echo "Ensuring proper linking..."
brew unlink protobuf && brew link protobuf
brew unlink grpc && brew link grpc

# Create fresh build directory
echo "Creating fresh build directory..."
mkdir -p build
cd build

# Configure with explicit paths
echo "Configuring CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DProtobuf_DIR=/opt/homebrew/lib/cmake/protobuf \
    -DgRPC_DIR=/opt/homebrew/lib/cmake/grpc

echo "Configuration completed successfully!"
echo ""
echo "You can now build the project with:"
echo "  make -j\$(nproc)"
echo ""
echo "If you still encounter protobuf conflicts, you can try:"
echo "  1. Using third-party gRPC: ./third_party/install_grpc.sh"
echo "  2. Or run: brew uninstall protobuf grpc && brew install protobuf grpc"
