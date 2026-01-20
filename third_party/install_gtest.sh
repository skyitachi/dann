#!/bin/bash

# Script to download and build Google Test as third-party dependency
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
GTEST_VERSION="1.14.0"
GTEST_URL="https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz"
THIRD_PARTY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GTEST_DIR="${THIRD_PARTY_DIR}/gtest"
BUILD_DIR="${GTEST_DIR}/build"
INSTALL_DIR="${GTEST_DIR}/install"

echo -e "${GREEN}Setting up Google Test v${GTEST_VERSION} as third-party dependency${NC}"

# Create directories
mkdir -p "${GTEST_DIR}"
mkdir -p "${BUILD_DIR}"
mkdir -p "${INSTALL_DIR}"

# Download gtest if not already present
if [ ! -f "${GTEST_DIR}/googletest-${GTEST_VERSION}.tar.gz" ]; then
    echo -e "${YELLOW}Downloading Google Test v${GTEST_VERSION}...${NC}"
    curl -L -o "${GTEST_DIR}/googletest-${GTEST_VERSION}.tar.gz" "${GTEST_URL}"
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Failed to download Google Test${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Google Test archive already exists, skipping download${NC}"
fi

# Extract if not already extracted
if [ ! -d "${GTEST_DIR}/googletest-${GTEST_VERSION}" ]; then
    echo -e "${YELLOW}Extracting Google Test...${NC}"
    cd "${GTEST_DIR}"
    tar -xzf "googletest-${GTEST_VERSION}.tar.gz"
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Failed to extract Google Test${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Google Test already extracted, skipping extraction${NC}"
fi

# Configure and build gtest
echo -e "${YELLOW}Configuring and building Google Test...${NC}"
cd "${BUILD_DIR}"

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DBUILD_GMOCK=ON \
      -DINSTALL_GTEST=ON \
      -DCMAKE_CXX_STANDARD=17 \
      -DCMAKE_CXX_STANDARD_REQUIRED=ON \
      "../googletest-${GTEST_VERSION}"

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to configure Google Test${NC}"
    exit 1
fi

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build Google Test${NC}"
    exit 1
fi

# Install
make install

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to install Google Test${NC}"
    exit 1
fi

# Create a simple test to verify installation
echo -e "${YELLOW}Verifying installation...${NC}"

if [ -f "${INSTALL_DIR}/lib/libgtest.a" ] && [ -f "${INSTALL_DIR}/lib/libgtest_main.a" ]; then
    echo -e "${GREEN}Google Test successfully installed!${NC}"
    echo -e "${GREEN}Installation directory: ${INSTALL_DIR}${NC}"
    echo -e "${GREEN}Libraries: $(ls "${INSTALL_DIR}/lib/" | grep gtest)${NC}"
    echo -e "${GREEN}Headers: $(ls "${INSTALL_DIR}/include/gtest/" | head -5)${NC}"
else
    echo -e "${RED}Google Test installation verification failed${NC}"
    exit 1
fi

# Create a README file
cat > "${GTEST_DIR}/README.md" << EOF
# Google Test Third-Party Dependency

This directory contains Google Test v${GTEST_VERSION} built as a third-party dependency for the DANN project.

## Installation Details

- **Version**: ${GTEST_VERSION}
- **Build Type**: Release
- **C++ Standard**: C++17
- **Installation Date**: $(date)

## Directory Structure

- \`install/\` - Built libraries and headers
  - \`include/\` - Header files
  - \`lib/\` - Static libraries (libgtest.a, libgtest_main.a, libgmock.a, libgmock_main.a)

## Usage in CMake

The CMakeLists.txt is configured to automatically detect and use this installation:

\`\`\`cmake
if(EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/third_party/gtest/install")
    set(GTEST_ROOT "\${CMAKE_CURRENT_SOURCE_DIR}/third_party/gtest/install")
    set(GTEST_INCLUDE_DIR "\${GTEST_ROOT}/include")
    set(GTEST_LIB_DIR "\${GTEST_ROOT}/lib")
    
    include_directories(\${GTEST_INCLUDE_DIR})
    link_directories(\${GTEST_LIB_DIR})
endif()
\`\`\`

## Build Notes

- Built with static libraries for easier linking
- Both gtest and gmock are included
- Built with Release configuration for optimal performance
- Uses C++17 standard to match project requirements

## Rebuilding

To rebuild Google Test:

1. Delete the \`install\` directory: \`rm -rf install/\`
2. Run this script again: \`./install_gtest.sh\`

EOF

echo -e "${GREEN}Setup complete! Google Test is ready to use as a third-party dependency.${NC}"
