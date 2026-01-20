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

# Core library sources
set(CORE_SOURCES
    src/core/vector_index.cpp
    src/core/node_manager.cpp
    src/core/consistency_manager.cpp
    src/core/query_router.cpp
    src/core/bulk_loader.cpp
    src/core/serialization.cpp
    src/generated/vector_service.pb.cc
    src/generated/node_management.pb.cc
    src/generated/consistency.pb.cc
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

# Utils sources
set(UTILS_SOURCES
    src/utils/logger.cpp
    src/utils/config.cpp
    src/utils/metrics.cpp
)

# Create core library
add_library(dann_core STATIC ${CORE_SOURCES})

# Link FAISS based on installation method
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/faiss/install")
    target_link_libraries(dann_core ${FAISS_LIB_DIR}/libfaiss.a)
else()
    target_link_libraries(dann_core ${FAISS_LIBRARIES})
endif()

target_link_libraries(dann_core ${CMAKE_THREAD_LIBS_INIT} ${OpenMP_CXX_LIBRARIES} protobuf)

# Generate protobuf files
add_custom_target(protobuf_files
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${PROTOBUF_LIBRARY_DIRS} ${PROTOBUF_PROTOC_EXECUTABLE}
    --cpp_out=src/generated --proto_path=proto proto/vector_service.proto
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${PROTOBUF_LIBRARY_DIRS} ${PROTOBUF_PROTOC_EXECUTABLE}
    --cpp_out=src/generated --proto_path=proto proto/node_management.proto
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${PROTOBUF_LIBRARY_DIRS} ${PROTOBUF_PROTOC_EXECUTABLE}
    --cpp_out=src/generated --proto_path=proto proto/consistency.proto
    COMMAND ${CMAKE_COMMAND} -E make_directory include/generated
    COMMAND ${CMAKE_COMMAND} -E rename src/generated/*.h include/generated/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating protobuf files"
)

# Make sure protobuf files are generated before building
add_dependencies(dann_core protobuf_files)

# Create network library
add_library(dann_network STATIC ${NETWORK_SOURCES})
target_link_libraries(dann_network dann_core grpc++ grpc++_reflection protobuf)

# Create storage library
add_library(dann_storage STATIC ${STORAGE_SOURCES})
target_link_libraries(dann_storage dann_core hiredis)

# Create utils library
add_library(dann_utils STATIC ${UTILS_SOURCES})
target_link_libraries(dann_utils spdlog)

# Main executable
add_executable(dann_server src/main.cpp)
target_link_libraries(dann_server dann_core dann_network dann_storage dann_utils)

# Test executable
add_executable(dann_test tests/test_main.cpp)
target_link_libraries(dann_test dann_core dann_network dann_storage dann_utils gtest gtest_main)

# Installation
install(TARGETS dann_server RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# Testing
enable_testing()
add_test(NAME DANNTests COMMAND dann_test)
