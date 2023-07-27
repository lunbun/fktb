cmake_minimum_required(VERSION 3.24)
project(fktb)

add_executable(fktb src/main.cc)

set_property(TARGET fktb PROPERTY CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(fktb PRIVATE -O0 -g -march=native -fno-rtti)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(fktb PRIVATE -Ofast -DNDEBUG -march=native -flto=auto -fno-rtti)
endif()

target_include_directories(fktb PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Embed the NNUE network into the binary
function(embed_nnue_network path)
    message("Embedding NNUE network ${path}")

    # Create a custom target that will generate the object file
    set(output "${CMAKE_BINARY_DIR}/nnue/network.o")
    target_link_libraries(fktb PRIVATE ${output})

    add_custom_command(
            OUTPUT ${output}
            COMMAND ${CMAKE_LINKER} --relocatable --format binary --output ${output} ${path}
            DEPENDS ${path}
    )

    add_custom_target(embedded_nnue_network DEPENDS ${output})
    add_dependencies(fktb embedded_nnue_network)

    # Generate the symbols to link to the embedded network, and add them as compile definitions
    string(MAKE_C_IDENTIFIER "${path}" network_identifier)
    set(network_start "_binary_${network_identifier}_start")
    set(network_end "_binary_${network_identifier}_end")
    set(network_size "_binary_${network_identifier}_size")
    target_compile_definitions(fktb PRIVATE
            NNUE_NETWORK_START=${network_start}
            NNUE_NETWORK_END=${network_end}
            NNUE_NETWORK_SIZE=${network_size})
endfunction()

embed_nnue_network("${CMAKE_SOURCE_DIR}/nnue/network.pt.bin")

add_subdirectory(src)
