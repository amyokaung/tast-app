cmake_minimum_required(VERSION 3.22.1)

project("myanmarofflineai")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
set(GGML_CUDA OFF CACHE BOOL "" FORCE)
set(GGML_METAL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(LLAMA_ROOT
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../cpp/llama.cpp
)

if(NOT EXISTS "${LLAMA_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "llama.cpp submodule is missing. Run: git submodule update --init --recursive"
    )
endif()

add_subdirectory(
    ${LLAMA_ROOT}
    ${CMAKE_CURRENT_BINARY_DIR}/llama-build
    EXCLUDE_FROM_ALL
)

add_library(
    llama_bridge
    SHARED
    llama_bridge.cpp
)

find_library(log-lib log)
find_library(android-lib android)

target_include_directories(
    llama_bridge
    PRIVATE
    ${LLAMA_ROOT}/include
)

target_link_libraries(
    llama_bridge
    llama
    ${log-lib}
    ${android-lib}
)
