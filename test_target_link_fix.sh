#!/bin/bash

# Test script to verify target_link_libraries fix
set -e

echo "Testing target_link_libraries signature fix..."

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

if [ $? -eq 0 ]; then
    echo "✓ CMake configuration successful!"
    
    # Try to build
    echo "Attempting to build..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    if [ $? -eq 0 ]; then
        echo "✓ Build successful!"
        
        if [ -f "dann_server" ]; then
            echo "✓ dann_server executable created"
            echo ""
            echo "All target_link_libraries signature issues have been fixed!"
        else
            echo "⚠ Build completed but executable not found"
        fi
    else
        echo "✗ Build failed"
        exit 1
    fi
else
    echo "✗ CMake configuration failed"
    exit 1
fi

echo ""
echo "Fix summary:"
echo "- All target_link_libraries calls now use consistent keyword syntax"
echo "- PRIVATE keyword used for all library dependencies"
echo "- CMake signature conflicts resolved"
