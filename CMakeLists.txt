cmake_minimum_required(VERSION 3.15)
project(fragarach)


set(CMAKE_CXX_STANDARD 17)

add_executable(fragarach
    src/main.cpp
    src/sandbox/sandbox.cpp
)


target_include_directories(fragarach PRIVATE src)