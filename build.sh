#!/bin/bash

set -e

echo "Building DANN - Distributed Approximate Nearest Neighbors"

# Check if build directory exists
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
make -j$(nproc)

# Run tests
echo "Running tests..."
if [ -f "dann_test" ]; then
    ./dann_test
fi

echo "Build completed successfully!"
echo "Executable: build/dann_server"
echo "Test executable: build/dann_test"
