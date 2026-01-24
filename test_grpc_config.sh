#!/bin/bash

# Test script to verify gRPC configuration
set -e

echo "Testing gRPC configuration for DANN..."

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

# Run CMake configuration
echo "Running CMake configuration..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Check if gRPC was found
if grep -q "Building with gRPC support" CMakeCache.txt || grep -q "Building with gRPC support" CMakeOutput.log 2>/dev/null; then
    echo "✓ gRPC configuration successful!"
    
    # Try to build
    echo "Attempting to build..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    if [ -f "dann_server" ]; then
        echo "✓ Build successful! dann_server created"
        
        # Check if gRPC symbols are present
        if command -v otool &> /dev/null; then
            echo "Checking for gRPC symbols..."
            if otool -L dann_server | grep -q grpc; then
                echo "✓ gRPC libraries linked successfully"
            else
                echo "⚠ Warning: gRPC libraries not found in executable"
            fi
        elif command -v ldd &> /dev/null; then
            echo "Checking for gRPC symbols..."
            if ldd dann_server | grep -q grpc; then
                echo "✓ gRPC libraries linked successfully"
            else
                echo "⚠ Warning: gRPC libraries not found in executable"
            fi
        fi
    else
        echo "✗ Build failed"
        exit 1
    fi
else
    echo "⚠ gRPC not found, building without gRPC support"
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

echo ""
echo "Configuration test completed!"
echo ""
echo "Summary:"
echo "- CMake configuration: ✓"
echo "- Build process: ✓"
echo "- gRPC integration: $([ -f dann_server ] && (otool -L dann_server 2>/dev/null | grep -q grpc || ldd dann_server 2>/dev/null | grep -q grpc) && echo "✓" || echo "⚠ Not available")"
