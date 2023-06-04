cmake_minimum_required(VERSION 3.24)
project(chess)

add_executable(chess src/main.cc)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -march=native")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -march=native -flto -fno-rtti")
endif()

target_include_directories(chess PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_subdirectory(src)
