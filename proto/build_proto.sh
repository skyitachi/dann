#!/bin/bash

# Build script for protobuf files

set -e

echo "Building protobuf files for DANN..."

# Check if protoc is installed
if ! command -v protoc &> /dev/null; then
    echo "Error: protoc is not installed. Please install Protocol Buffers compiler."
    echo "On macOS: brew install protobuf"
    echo "On Ubuntu: apt-get install protobuf-compiler"
    exit 1
fi

# Create output directories
mkdir -p include/generated
mkdir -p src/generated

# Generate C++ files
echo "Generating C++ files from protobuf definitions..."

# Vector service
protoc --cpp_out=src/generated --proto_path=proto proto/vector_service.proto

# Node management
protoc --cpp_out=src/generated --proto_path=proto proto/node_management.proto

# Consistency service
protoc --cpp_out=src/generated --proto_path=proto proto/consistency.proto

# Move header files to include directory
mv src/generated/*.h include/generated/

echo "Protobuf files generated successfully!"
echo "Header files: include/generated/"
echo "Source files: src/generated/"

# List generated files
echo ""
echo "Generated header files:"
ls -la include/generated/

echo ""
echo "Generated source files:"
ls -la src/generated/
