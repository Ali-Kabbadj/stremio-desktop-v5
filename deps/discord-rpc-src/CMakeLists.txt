cmake_minimum_required(VERSION 3.16)
project(discord-rpc-lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED) 

set(LIB_SOURCES
    src/command-queue.cpp
    src/discord-rpc.cpp
    src/platform/windows.cpp
    src/serialization.cpp
)

add_library(discord-rpc-static STATIC ${LIB_SOURCES})

target_include_directories(discord-rpc-static
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(discord-rpc-static
    PUBLIC
        fmt::fmt
        glaze::glaze 
        ws2_32.lib
)