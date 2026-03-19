cmake_minimum_required(VERSION 3.15)
project(fragarach)


set(CMAKE_CXX_STANDARD 17)

add_executable(fragarach
    src/main.cpp
    src/sandbox/sandbox.cpp
    src/tracer/tracer.cpp
)



add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tracer.bpf.o
    COMMAND clang -g -O2 -target bpf -c ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf/tracer.bpf.c -o ${CMAKE_CURRENT_BINARY_DIR}/tracer.bpf.o
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf/tracer.bpf.c
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tracer.skel.h
    COMMAND bpftool gen skeleton ${CMAKE_CURRENT_BINARY_DIR}/tracer.bpf.o > ${CMAKE_CURRENT_BINARY_DIR}/tracer.skel.h
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tracer.bpf.o
)

add_custom_target(bpf_tracer
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tracer.skel.h
)

add_dependencies(fragarach bpf_tracer)

target_include_directories(fragarach PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(fragarach PRIVATE bpf elf z)