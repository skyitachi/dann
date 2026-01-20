cmake_minimum_required(VERSION 3.16)
project(DANN VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    execute_process(
        COMMAND brew --prefix libomp
        OUTPUT_VARIABLE LIBOMP_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(LIBOMP_PREFIX)
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib")
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_C_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib")
    endif()
endif()
find_package(OpenMP REQUIRED)

# Find protobuf
find_package(Protobuf REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/faiss)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# Add subdirectories
# Option 1: Use system-installed FAISS (recommended)
# add_subdirectory(third_party/faiss)

# Option 2: Use local FAISS build
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/faiss/install")
    set(FAISS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/faiss/install")
    set(FAISS_INCLUDE_DIR "${FAISS_ROOT}/include")
    set(FAISS_LIB_DIR "${FAISS_ROOT}/lib")
    
    include_directories(${FAISS_INCLUDE_DIR})
    link_directories(${FAISS_LIB_DIR})
else()
    # Try to find system FAISS
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FAISS REQUIRED faiss)
    
    if(FAISS_FOUND)
        include_directories(${FAISS_INCLUDE_DIRS})
        link_directories(${FAISS_LIBRARY_DIRS})
    else()
        message(FATAL_ERROR "FAISS not found. Please install FAISS or run third_party/install_faiss.sh")
    endif()
endif()

# Find or use third-party gtest
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/gtest/install")
    set(GTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/gtest/install")
    set(GTEST_INCLUDE_DIR "${GTEST_ROOT}/include")
    set(GTEST_LIB_DIR "${GTEST_ROOT}/lib")
    
    include_directories(${GTEST_INCLUDE_DIR})
    link_directories(${GTEST_LIB_DIR})
    message(STATUS "Using third-party gtest from: ${GTEST_ROOT}")
else()
    # Try to find system gtest
    find_package(GTest REQUIRED)
    message(STATUS "Using system gtest")
endif()

# Core library sources (minimal version without protobuf for testing)
set(CORE_SOURCES_MINIMAL
    src/core/vector_index.cpp
)

# Create minimal core library for testing
add_library(dann_core_minimal STATIC ${CORE_SOURCES_MINIMAL})

# Add OpenMP support for macOS
if(APPLE)
    target_compile_options(dann_core_minimal PRIVATE -Xpreprocessor -fopenmp)
    target_include_directories(dann_core_minimal PRIVATE /opt/homebrew/opt/libomp/include)
else()
    target_compile_options(dann_core_minimal PRIVATE ${OpenMP_CXX_FLAGS})
    target_include_directories(dann_core_minimal PRIVATE ${OpenMP_CXX_INCLUDE_DIRS})
endif()

# Link FAISS based on installation method
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/faiss/install")
    target_link_libraries(dann_core_minimal ${FAISS_LIB_DIR}/libfaiss.a)
else()
    target_link_libraries(dann_core_minimal ${FAISS_LIBRARIES})
endif()

if(APPLE)
    target_link_libraries(dann_core_minimal ${CMAKE_THREAD_LIBS_INIT} /opt/homebrew/opt/libomp/lib/libomp.dylib "-framework Accelerate")
else()
    target_link_libraries(dann_core_minimal ${CMAKE_THREAD_LIBS_INIT} ${OpenMP_CXX_LIBRARIES})
endif()

# Network layer sources
set(NETWORK_SOURCES
    src/network/rpc_server.cpp
    src/network/rpc_client.cpp
    src/network/message_handler.cpp
)

# Storage layer sources
set(STORAGE_SOURCES
    src/storage/redis_client.cpp
    src/storage/local_storage.cpp
)

# Utils sources
set(UTILS_SOURCES
    src/utils/logger.cpp
    src/utils/config.cpp
    src/utils/metrics.cpp
)

# Network layer sources
set(NETWORK_SOURCES
    src/network/rpc_server.cpp
    src/network/rpc_client.cpp
    src/network/message_handler.cpp
)

# Storage layer sources
set(STORAGE_SOURCES
    src/storage/redis_client.cpp
    src/storage/local_storage.cpp
)

# Create utils library
add_library(dann_utils STATIC ${UTILS_SOURCES})
target_link_libraries(dann_utils spdlog)

# Main executable (commented out until core library is fixed)
# add_executable(dann_server src/main.cpp)
# target_link_libraries(dann_server dann_core dann_network dann_storage dann_utils)

# Test executable (minimal version without protobuf dependencies)
#add_executable(dann_test tests/gtest_integration_test.cpp)
add_executable(dann_test tests/vector_index_test.cpp)

# Link gtest based on installation method
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/gtest/install")
    target_link_libraries(dann_test 
        ${GTEST_LIB_DIR}/libgtest.a ${GTEST_LIB_DIR}/libgtest_main.a pthread dann_core_minimal)
else()
    target_link_libraries(dann_test gtest gtest_main dann_core_minimal)
endif()

# Installation (commented out until dann_server is rebuilt)
# install(TARGETS dann_server RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# Testing
enable_testing()
add_test(NAME DANNTests COMMAND dann_test)
