cmake_minimum_required(VERSION 3.8)

project(dabu_cli C)

option(FUZZ "Enable AFL++ fuzzing instrumentation" OFF)

set(name "dabu_cli")

if(FUZZ)
    message(STATUS "Fuzzing enabled: using AFL++ compilers")
    set(CMAKE_C_COMPILER "afl-clang-fast")
    set(CMAKE_CXX_COMPILER "afl-clang-fast++")
    set(name "fuzz_dabu_cli")
endif()

set(src main.c ../lz4.c ../dabu.c)

add_executable(${name} ${src})
