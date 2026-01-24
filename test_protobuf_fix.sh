#!/bin/bash

# Test script to verify protobuf compilation fix
set -e

echo "Testing protobuf compilation fix..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the DANN root directory"
    exit 1
fi

# Clean build directory
echo "Cleaning build directory..."
rm -rf build

# Create build directory
mkdir -p build
cd build

# Check for required tools
echo "Checking for required tools..."

# Check protoc
if command -v protoc &> /dev/null; then
    echo "✓ protoc found: $(protoc --version)"
else
    echo "✗ protoc not found"
    echo "Please install protobuf compiler:"
    echo "  brew install protobuf"
    exit 1
fi

# Check grpc_cpp_plugin
if command -v grpc_cpp_plugin &> /dev/null; then
    echo "✓ grpc_cpp_plugin found"
else
    echo "✗ grpc_cpp_plugin not found"
    echo "Please install gRPC tools:"
    echo "  brew install grpc"
    exit 1
fi

# Run CMake configuration
echo "Running CMake configuration..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Check if protobuf generation was successful
echo "Checking protobuf generation..."
if ls include/generated/*.pb.h 1> /dev/null 2>&1 && ls include/generated/*.grpc.pb.h 1> /dev/null 2>&1; then
    echo "✓ Protobuf files generated successfully"
    echo "Generated files:"
    ls -la include/generated/
else
    echo "✗ Protobuf files not generated"
    exit 1
fi

# Try to build
echo "Attempting to build..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [ -f "dann_server" ]; then
    echo "✓ Build successful! dann_server created"
else
    echo "✗ Build failed"
    exit 1
fi

echo ""
echo "✓ All tests passed!"
echo ""
echo "The protobuf compilation issue has been fixed."
echo "You can now build the project normally with:"
echo "  cd build && make -j\$(nproc)"
