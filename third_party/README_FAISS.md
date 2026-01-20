# FAISS 依赖安装指南

FAISS (Facebook AI Similarity Search) 是DANN项目的核心向量索引库。

## 安装方法

### 方法1: 使用包管理器安装 (推荐)

#### macOS (Homebrew)
```bash
brew install faiss
```

#### Ubuntu/Debian
```bash
# 安装依赖
sudo apt-get update
sudo apt-get install build-essential cmake libopenblas-dev liblapack-dev

# 安装FAISS
sudo apt-get install libfaiss-dev
```

#### CentOS/RHEL
```bash
# 安装EPEL
sudo yum install epel-release
sudo yum install faiss-devel
```

### 方法2: 从源码编译安装

#### 1. 克隆FAISS仓库
```bash
cd third_party
git clone https://github.com/facebookresearch/faiss.git
```

#### 2. 安装编译依赖
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libopenblas-dev liblapack-dev

# macOS
brew install cmake openblas lapack
```

#### 3. 编译FAISS
```bash
cd faiss
mkdir build && cd build

# 配置编译选项
cmake .. \
  -DFAISS_ENABLE_GPU=OFF \
  -DFAISS_ENABLE_PYTHON=OFF \
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local

# 编译
make -j$(nproc)

# 安装 (可选)
sudo make install
```

### 方法3: 使用Conda安装
```bash
conda install -c conda-forge faiss-cpu
# 或者GPU版本
conda install -c conda-forge faiss-gpu
```

### 方法4: 使用pip安装
```bash
pip install faiss-cpu
# 或者GPU版本
pip install faiss-gpu
```

## DANN项目集成

### 选项A: 系统安装的FAISS

如果使用包管理器安装，FAISS会安装在系统路径中，CMake会自动找到：

```bash
# 安装后验证
pkg-config --modversion faiss
```

### 选项B: 第三方源码集成

1. 将FAISS源码放在third_party/faiss目录
2. 修改CMakeLists.txt使用本地FAISS：

```cmake
# 如果使用本地FAISS
set(FAISS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/faiss)
add_subdirectory(${FAISS_SOURCE_DIR})

# 链接FAISS
target_link_libraries(dann_core faiss)
```

### 选项C: 预编译库集成

1. 下载预编译的FAISS库
2. 放置在third_party/faiss目录
3. 配置CMakeLists.txt指向预编译库

## 编译配置选项

### CPU版本 (推荐用于DANN)
```bash
cmake .. \
  -DFAISS_ENABLE_GPU=OFF \
  -DFAISS_ENABLE_PYTHON=OFF \
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release
```

### GPU版本 (如果需要GPU加速)
```bash
cmake .. \
  -DFAISS_ENABLE_GPU=ON \
  -DCUDA_TOOLKIT_ROOT_DIR=/usr/local/cuda \
  -DFAISS_ENABLE_PYTHON=OFF \
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release
```

## 验证安装

### 检查FAISS版本
```cpp
#include <faiss/Index.h>
#include <iostream>

int main() {
    std::cout << "FAISS version: " << faiss::version() << std::endl;
    return 0;
}
```

### 编译测试
```bash
g++ -std=c++11 -I/usr/include/faiss test.cpp -lfaiss -o test
./test
```

## 常见问题

### 1. 找不到FAISS
```bash
# 检查pkg-config
pkg-config --cflags --libs faiss

# 手动指定路径
export CMAKE_PREFIX_PATH=/usr/local
```

### 2. BLAS/LAPACK依赖问题
```bash
# Ubuntu
sudo apt-get install libopenblas-dev liblapack-dev

# macOS
brew install openblas lapack
```

### 3. 编译错误
```bash
# 清理重新编译
cd faiss/build
make clean
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 性能优化

### 1. OpenBLAS优化
```bash
# 使用OpenBLAS替代默认BLAS
cmake .. -DBLA_VENDOR=OpenBLAS
```

### 2. 编译器优化
```bash
# 启用所有优化
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native"
```

### 3. 内存对齐
```bash
cmake .. -DFAISS_OPT_LEVEL=3
```

## Docker集成

如果使用Docker，可以在Dockerfile中安装FAISS：

```dockerfile
FROM ubuntu:20.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libopenblas-dev \
    liblapack-dev \
    git

RUN cd /opt && \
    git clone https://github.com/facebookresearch/faiss.git && \
    cd faiss && \
    mkdir build && cd build && \
    cmake .. -DFAISS_ENABLE_GPU=OFF -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install
```

## 版本兼容性

- FAISS 1.7.4+ (推荐)
- C++17+
- OpenBLAS 0.3.10+
- CMake 3.16+

## 推荐配置

对于DANN项目，推荐使用：

1. **FAISS版本**: 1.7.4
2. **编译选项**: CPU版本，Release模式
3. **BLAS库**: OpenBLAS
4. **安装方式**: 系统包管理器或源码编译

这样可以获得最佳的性能和稳定性。
