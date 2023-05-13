cmake_minimum_required(VERSION 3.24)
project(chess)

add_executable(chess src/main.cc)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

set_property(TARGET chess PROPERTY CMAKE_CXX_STANDARD 17)
set_property(TARGET chess PROPERTY CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -O3")

target_include_directories(chess PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_subdirectory(src)

target_link_libraries(chess)
