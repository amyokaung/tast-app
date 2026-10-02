cmake_minimum_required(VERSION 3.22.1)

project("myanmarofflineai")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# llama.cpp / ggml Android configuration
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
set(GGML_CUDA OFF CACHE BOOL "" FORCE)
set(GGML_METAL OFF CACHE BOOL "" FORCE)
set(GGML_LLAMAFILE OFF CACHE BOOL "" FORCE)

set(LLAMA_OPENSSL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Repository root:
# android/app/src/main/cpp
#        ../../../../../
# gives project root.
set(LLAMA_ROOT
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../cpp/llama.cpp
)

# llama.cpp is downloaded by GitHub Actions before Gradle build.
if(NOT EXISTS "${LLAMA_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "llama.cpp is missing. "
        "Run: git clone --depth 1 https://github.com/ggml-org/llama.cpp.git cpp/llama.cpp"
    )
endif()

if(NOT EXISTS "${LLAMA_ROOT}/include/llama.h")
    message(FATAL_ERROR
        "llama.h is missing from cpp/llama.cpp/include/"
    )
endif()

message(STATUS "Using llama.cpp from: ${LLAMA_ROOT}")

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
