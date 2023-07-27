cmake_minimum_required(VERSION 3.24)
project(fktb)

add_executable(fktb src/main.cc)

set_property(TARGET fktb PROPERTY CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(fktb PRIVATE -O0 -g -march=native -fno-rtti)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(fktb PRIVATE -Ofast -DNDEBUG -march=native -flto=auto -fno-rtti)
endif()

target_include_directories(fktb PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_subdirectory(src)
