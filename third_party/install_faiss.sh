#!/bin/bash

# FAISS installation script for DANN project

set -e

FAISS_VERSION="1.7.4"
INSTALL_DIR="$(pwd)/third_party/faiss"
BUILD_TYPE="Release"
ENABLE_GPU="OFF"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            FAISS_VERSION="$2"
            shift 2
            ;;
        --gpu)
            ENABLE_GPU="ON"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --help)
            echo "Usage: $0 [--version VERSION] [--gpu] [--debug] [--help]"
            echo "  --version VERSION  Set FAISS version (default: 1.7.4)"
            echo "  --gpu             Enable GPU support"
            echo "  --debug           Build in debug mode"
            echo "  --help            Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "Installing FAISS for DANN..."
echo "Version: $FAISS_VERSION"
echo "GPU Support: $ENABLE_GPU"
echo "Build Type: $BUILD_TYPE"

# Check if directory already exists
if [ -d "$INSTALL_DIR" ]; then
    echo "FAISS directory already exists. Removing..."
    rm -rf "$INSTALL_DIR"
fi

# Install dependencies based on OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    echo "Installing dependencies for macOS..."
    if ! command -v brew &> /dev/null; then
        echo "Error: Homebrew not found. Please install Homebrew first."
        exit 1
    fi
    
    brew install cmake openblas lapack libomp
    
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux
    echo "Installing dependencies for Linux..."
    
    if command -v apt-get &> /dev/null; then
        # Ubuntu/Debian
        sudo apt-get update
        sudo apt-get install -y build-essential cmake libopenblas-dev liblapack-dev git
    elif command -v yum &> /dev/null; then
        # CentOS/RHEL
        sudo yum groupinstall -y "Development Tools"
        sudo yum install -y cmake openblas-devel lapack-devel git
    else
        echo "Error: Unsupported Linux distribution"
        exit 1
    fi
else
    echo "Error: Unsupported operating system: $OSTYPE"
    exit 1
fi

# Clone FAISS
echo "Cloning FAISS $FAISS_VERSION..."
git clone --depth 1 --branch v$FAISS_VERSION https://github.com/facebookresearch/faiss.git "$INSTALL_DIR"

# Build FAISS
echo "Building FAISS..."
cd "$INSTALL_DIR"
mkdir -p build
cd build

# Configure CMake
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE
    -DFAISS_ENABLE_GPU=$ENABLE_GPU
    -DFAISS_ENABLE_PYTHON=OFF
    -DBUILD_TESTING=OFF
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/install"
)

# Add OpenBLAS path on macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OPENBLAS_PATH=$(brew --prefix openblas 2>/dev/null || echo "/usr/local")
    OPENBLAS_LIB="${OPENBLAS_PATH}/lib/libopenblas.dylib"
    CMAKE_ARGS+=(
        -DBLA_VENDOR=OpenBLAS
        -DBLAS_ROOT="$OPENBLAS_PATH"
        -DBLAS_LIBRARIES="$OPENBLAS_LIB"
        -DLAPACK_LIBRARIES="$OPENBLAS_LIB"
    )
    LIBOMP_PREFIX=$(brew --prefix libomp 2>/dev/null || echo "/usr/local/opt/libomp")
    if [ -d "$LIBOMP_PREFIX" ]; then
        CMAKE_ARGS+=(
            -DOpenMP_CXX_FLAGS="-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include"
            -DOpenMP_CXX_LIB_NAMES=omp
            -DOpenMP_omp_LIBRARY="${LIBOMP_PREFIX}/lib/libomp.dylib"
        )
    fi
fi

echo "Running CMake with arguments: ${CMAKE_ARGS[@]}"
cmake .. "${CMAKE_ARGS[@]}"

# Compile
echo "Compiling FAISS..."
make -j$(nproc)

# Install to local directory
echo "Installing FAISS..."
make install

# Create pkg-config file
echo "Creating pkg-config file..."
mkdir -p "$INSTALL_DIR/lib/pkgconfig"
cat > "$INSTALL_DIR/lib/pkgconfig/faiss.pc" << EOF
prefix=$INSTALL_DIR/install
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: FAISS
Description: Facebook AI Similarity Search
Version: $FAISS_VERSION
Libs: -L\${libdir} -lfaiss
Cflags: -I\${includedir}
EOF

echo ""
echo "FAISS installation completed successfully!"
echo "Installation directory: $INSTALL_DIR"
echo ""
echo "To use FAISS with DANN, set the following environment variables:"
echo "export FAISS_ROOT=$INSTALL_DIR/install"
echo "export PKG_CONFIG_PATH=$INSTALL_DIR/lib/pkgconfig:\$PKG_CONFIG_PATH"
echo ""
echo "Or update CMakeLists.txt to use this FAISS installation:"
echo "set(FAISS_ROOT $INSTALL_DIR/install)"
echo "find_package(FAISS REQUIRED HINTS \${FAISS_ROOT})"
echo ""

# Test installation
echo "Testing FAISS installation..."
cd ../../

# Compile test
g++ -std=c++11 -I"$INSTALL_DIR/install/include" test_faiss.cpp -L"$INSTALL_DIR/install/lib" -lfaiss -o test_faiss

# Run test
if ./test_faiss; then
    echo "FAISS test: PASSED"
else
    echo "FAISS test: FAILED"
    exit 1
fi

# Cleanup
rm -f test_faiss.cpp test_faiss

echo ""
echo "FAISS is ready for use with DANN!"
