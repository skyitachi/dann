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

# Set OpenMP include directories for all targets
if(APPLE AND LIBOMP_PREFIX)
    include_directories(${LIBOMP_PREFIX}/include)
    link_directories(${LIBOMP_PREFIX}/lib)
endif()

# Find protobuf - use standard mode to get protobuf_generate_cpp function
find_package(Protobuf CONFIG REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/generated)
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
    src/core/index.cpp
    src/core/ivf_index.cpp
    src/core/clustering.cpp
    src/core/distributed_index_ivf.cpp
        src/core/ivf_shard.cpp
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

# Add BLAS/LAPACK for Linux systems
if(UNIX AND NOT APPLE)
    find_package(BLAS REQUIRED)
    find_package(LAPACK REQUIRED)
    target_link_libraries(dann_core_minimal ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})
endif()

if(APPLE)
    target_link_libraries(dann_core_minimal ${CMAKE_THREAD_LIBS_INIT} /opt/homebrew/opt/libomp/lib/libomp.dylib "-framework Accelerate")
else()
    target_link_libraries(dann_core_minimal ${CMAKE_THREAD_LIBS_INIT} ${OpenMP_CXX_LIBRARIES})
endif()

# Network layer sources
set(NETWORK_SOURCES
    src/network/rpc_server.cpp
#    src/network/rpc_client.cpp
#    src/network/message_handler.cpp
    src/network/vector_search_service_impl.h
    src/network/vector_search_service_impl.cpp
)

# Storage layer sources
#set(STORAGE_SOURCES
#    src/storage/redis_client.cpp
#    src/storage/local_storage.cpp
#)

# Utils sources
set(UTILS_SOURCES
    src/utils/logger.cpp
    src/utils/config.cpp
    src/utils/metrics.cpp
    src/utils/util.cpp
)

# Create utils library
add_library(dann_utils STATIC ${UTILS_SOURCES})
#target_link_libraries(dann_utils spdlog)


# Try to find gRPC and protobuf (optional)
# Use multiple approaches to avoid target conflicts

# Approach 1: Try pkg-config first (most reliable)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(GRPC QUIET grpc++)
    pkg_check_modules(PROTOBUF QUIET protobuf)

    if(GRPC_FOUND AND PROTOBUF_FOUND)
        message(STATUS "Building with gRPC support (pkg-config)")
        set(GRPC_FOUND_PKGCONFIG TRUE)
        set(Protobuf_FOUND TRUE)
        set(gRPC_FOUND TRUE)
    endif()
endif()

# Approach 2: Try Homebrew with explicit paths (if pkg-config failed)
if(NOT gRPC_FOUND AND EXISTS "/opt/homebrew")
    message(STATUS "Attempting Homebrew gRPC configuration...")
    
    # Set explicit paths to avoid conflicts
    set(CMAKE_PREFIX_PATH "/opt/homebrew;${CMAKE_PREFIX_PATH}")
    
    # Try to find gRPC without protobuf dependency
    find_package(gRPC QUIET CONFIG PATHS "/opt/homebrew")
    
    if(gRPC_FOUND)
        message(STATUS "Building with gRPC support (Homebrew explicit)")
        set(GRPC_FOUND_SYSTEM TRUE)
        
        # Find protobuf separately
        find_package(Protobuf QUIET PATHS "/opt/homebrew")
        if(Protobuf_FOUND)
            message(STATUS "Protobuf found successfully")
        else()
            message(WARNING "Protobuf not found, gRPC may not work properly")
        endif()
    endif()
endif()

if(NOT gRPC_FOUND)
    message(STATUS "Building without gRPC support (gRPC not found)")
endif()

# Main executable
add_executable(dann_server src/main.cpp)
target_link_libraries(dann_server 
    PRIVATE
        dann_core_minimal 
        dann_utils
)

if(Protobuf_FOUND AND gRPC_FOUND)
    # Generate protobuf files
    set(PROTO_FILES
        proto/vector_service.proto
    )

    add_library(myproto ${PROTO_FILES})

    set_target_properties(myproto PROPERTIES LINKER_LANGUAGE CXX)

    # Generated headers are produced under the build directory (${binary_dir}/proto).
    # Keep this include dir on myproto so consumers can include "vector_service.pb.h".
    target_include_directories(myproto PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto)

    # Ensure protobuf headers (e.g. google/protobuf/runtime_version.h) are visible when compiling
    # generated *.pb.cc sources.
    if(TARGET protobuf::libprotobuf)
        target_link_libraries(myproto PUBLIC protobuf::libprotobuf)
    elseif(GRPC_FOUND_PKGCONFIG)
        target_include_directories(myproto PUBLIC ${PROTOBUF_INCLUDE_DIRS})
        target_link_libraries(myproto PUBLIC ${PROTOBUF_LIBRARIES})
    endif()

#    # Create generated include directory
#    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/generated)
    
    # Generate protobuf and gRPC files
    if(GRPC_FOUND_SYSTEM)
        target_link_libraries(myproto
                PUBLIC
                gRPC::grpc
                gRPC::grpc++
        )
        get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
        protobuf_generate(TARGET myproto LANGUAGE cpp
            IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
            PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto"
        )
        protobuf_generate(TARGET myproto LANGUAGE grpc
            IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
            PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto"
            GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
            PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
        )
    else()
        # If gRPC wasn't found as a CMake package target, fall back to protoc/grpc_cpp_plugin in PATH.
        # This supports pkg-config based setups.
        if(GRPC_FOUND_PKGCONFIG)
            # Use pkg-config found protoc and grpc_cpp_plugin
            message(STATUS "compile proto by pkgconfig")
            find_package(OpenSSL REQUIRED)
            find_program(PROTOC_EXECUTABLE protoc)
            find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
            if(NOT PROTOC_EXECUTABLE)
                message(FATAL_ERROR "protoc not found in PATH")
            endif()
            if(NOT GRPC_CPP_PLUGIN_EXECUTABLE)
                message(FATAL_ERROR "grpc_cpp_plugin not found in PATH")
            endif()
            protobuf_generate(TARGET myproto LANGUAGE cpp
                IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
                PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto"
                PROTOC_EXE "${PROTOC_EXECUTABLE}"
            )
            protobuf_generate(
                TARGET myproto
                LANGUAGE grpc
                IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
                GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
                PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
                PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto"
                PROTOC_EXE "${PROTOC_EXECUTABLE}"
            )
            
            # Link myproto with gRPC/Protobuf/OpenSSL dependencies
            target_include_directories(myproto PUBLIC ${GRPC_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
            target_link_directories(myproto PUBLIC ${GRPC_LIBRARY_DIRS} ${PROTOBUF_LIBRARY_DIRS})
            target_link_libraries(myproto
                PUBLIC
                    ${GRPC_LIBRARIES}
                    ${PROTOBUF_LIBRARIES}
                    pthread
                    OpenSSL::SSL
                    OpenSSL::Crypto
            )
        else()
            # Fall back to PATH search (for third-party or manual setups)
            find_program(PROTOC_EXECUTABLE protoc)
            find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
            if(NOT PROTOC_EXECUTABLE)
                message(FATAL_ERROR "protoc not found in PATH")
            endif()
            if(NOT GRPC_CPP_PLUGIN_EXECUTABLE)
                message(FATAL_ERROR "grpc_cpp_plugin not found in PATH")
            endif()

            protobuf_generate(TARGET myproto LANGUAGE cpp
                IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
                PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto"
                PROTOC_EXE "${PROTOC_EXECUTABLE}"
            )
            protobuf_generate(
                TARGET myproto
                LANGUAGE grpc
                IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
                GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
                PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
                PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto"
                PROTOC_EXE "${PROTOC_EXECUTABLE}"
            )
        endif()
    endif()
    
    # Create protobuf library
#    add_library(dann_proto STATIC ${ALL_PROTO_SRCS})
#    target_include_directories(dann_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/generated)
    
    # Link protobuf libraries to main executable

    # Add network sources to main executable
    target_sources(dann_server PRIVATE ${NETWORK_SOURCES})

    # Ensure dann_server can include generated headers (vector_service.pb.h / .grpc.pb.h)
    # by inheriting myproto's generated include dirs.
    target_link_libraries(dann_server PRIVATE myproto)

    if(GRPC_FOUND_PKGCONFIG)
        target_include_directories(dann_server PRIVATE ${GRPC_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
        target_link_directories(dann_server PRIVATE ${GRPC_LIBRARY_DIRS} ${PROTOBUF_LIBRARY_DIRS})
    endif()

    # Add compile definition for gRPC support
    target_compile_definitions(dann_server PUBLIC HAVE_GRPC)
    
else()
    message(STATUS "Building without gRPC support (gRPC/Protobuf not found)")
    
    # Create a stub implementation for missing components
    target_sources(dann_server PRIVATE 
        src/core/vector_index.cpp
        src/utils/logger.cpp
        src/utils/config.cpp
        src/utils/metrics.cpp
    )
    
    # Add compile definition for no gRPC
    target_compile_definitions(dann_server PUBLIC NO_GRPC)
endif()

# Test executable (minimal version without protobuf dependencies)
#add_executable(dann_test tests/gtest_integration_test.cpp)
add_executable(dann_test tests/vector_index_test.cpp tests/clustering_test.cpp)

# Link gtest based on installation method
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/gtest/install")
    target_link_libraries(dann_test 
        ${GTEST_LIB_DIR}/libgtest.a ${GTEST_LIB_DIR}/libgtest_main.a pthread dann_core_minimal dann_utils)
else()
    target_link_libraries(dann_test gtest gtest_main dann_core_minimal dann_utils)
endif()

# Installation (commented out until dann_server is rebuilt)
# install(TARGETS dann_server RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# Testing
enable_testing()
add_test(NAME DANNTests COMMAND dann_test)

# gRPC test client
if(Protobuf_FOUND AND gRPC_FOUND)
    add_executable(test_grpc_client test_grpc_client.cpp)
    target_link_libraries(test_grpc_client PRIVATE myproto)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(test_grpc_client PRIVATE -Wl,--no-as-needed)
    endif()

    find_package(HDF5 REQUIRED COMPONENTS CXX)
    target_include_directories(test_grpc_client PRIVATE ${HDF5_INCLUDE_DIRS})
    target_link_libraries(test_grpc_client PRIVATE ${HDF5_CXX_LIBRARIES})

    # Link gRPC libraries based on how they were found
    if(GRPC_FOUND_SYSTEM)
        # myproto already links gRPC/protobuf as PUBLIC deps in system mode;
        # avoid duplicating libraries on the final link line.
    elseif(GRPC_FOUND_PKGCONFIG)
        # myproto already links gRPC/protobuf as PUBLIC deps in pkg-config mode;
        # avoid duplicating libraries on the final link line.
        # Ensure OpenSSL comes after static gRPC libs on the final link line (Linux only)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(test_grpc_client PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        endif()
    endif()

    # HDF5 depends on zlib, ensure it's linked (Linux only)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(ZLIB REQUIRED)
        target_link_libraries(test_grpc_client PRIVATE ZLIB::ZLIB)
    endif()
endif()
