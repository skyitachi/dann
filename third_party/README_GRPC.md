# gRPC Dependency for DANN

This directory contains the gRPC and protobuf libraries required for DANN's gRPC functionality.

## Supported Platforms

- **macOS** (10.15+)
- **Linux** (Ubuntu, Debian, CentOS, RHEL)

## Installation

### macOS

Prerequisites:
- Xcode Command Line Tools: `xcode-select --install`
- cmake: `brew install cmake`
- git: `brew install git` (or included with Xcode)

Install gRPC and protobuf:
```bash
cd third_party
./install_grpc.sh
```

### Linux

Prerequisites (Ubuntu/Debian):
```bash
sudo apt-get update
sudo apt-get install cmake git build-essential
```

Prerequisites (CentOS/RHEL):
```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake git
```

Install gRPC and protobuf:
```bash
cd third_party
./install_grpc.sh
```

## Installation Process

The script will:
1. **Detect your operating system** (macOS/Linux)
2. **Check system dependencies** (cmake, git, Xcode tools on macOS)
3. **Download gRPC v1.54.3 and protobuf v23.4**
4. **Build them from source** with platform-specific optimizations
5. **Install to `third_party/grpc/install`**

## macOS-Specific Features

- **Automatic CPU detection**: Uses `sysctl -n hw.ncpu` for optimal parallel builds
- **Xcode tool verification**: Ensures Xcode Command Line Tools are installed
- **Deployment target**: Sets minimum macOS version to 10.15 for compatibility
- **Homebrew integration**: Detects and reports Homebrew availability

## Usage

Once installed, the DANN build system will automatically detect and use the third-party gRPC installation:

```bash
mkdir build && cd build
cmake ..
make -j4
```

The resulting `dann_server` will include gRPC functionality and accept gRPC connections on the configured port (default: 50051).

## Directory Structure

```
third_party/grpc/
├── install/          # Installation directory
│   ├── bin/         # protoc and grpc_cpp_plugin tools
│   ├── include/     # Header files
│   ├── lib/         # Static libraries
│   └── share/       # CMake config files
├── build/           # Build directory (temporary)
└── install_grpc.sh  # Installation script
```

## Version Information

- gRPC: 1.54.3
- protobuf: 23.4

These versions are tested and known to work with DANN on both macOS and Linux.

## Troubleshooting

### macOS Specific Issues

**Xcode Command Line Tools Missing:**
```bash
xcode-select --install
```

**Permission Issues:**
```bash
chmod +x install_grpc.sh
```

**Build Fails on macOS:**
- Ensure you have enough disk space (at least 2GB)
- Check that Xcode Command Line Tools are properly installed
- Try cleaning the build directory: `rm -rf third_party/grpc/build`

### General Issues

**Build Fails:**
- Ensure cmake and git are installed and in your PATH
- Make sure you have sufficient disk space (at least 2GB)
- Try cleaning the build directory: `rm -rf third_party/grpc/build`

**Linking Errors:**
- Verify the installation completed successfully
- Check that `${GRPC_ROOT}/lib/libgrpc++.a` exists
- Make sure you're using the correct build configuration

### Performance

The static libraries are built in Release mode for optimal performance. The script automatically detects your CPU count for parallel builds:
- macOS: Uses `sysctl -n hw.ncpu`
- Linux: Uses `nproc`

## System Installation Alternative

If you prefer to use system-installed gRPC packages, you can install them via your system package manager:

```bash
# macOS with Homebrew
brew install grpc protobuf

# Ubuntu/Debian
sudo apt-get install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc

# CentOS/RHEL
sudo yum install grpc-devel protobuf-devel protobuf-compiler-grpc
```

The build system will automatically detect and use system installations if available.
