cmake_minimum_required(VERSION 3.8)

project(dabu_cli C)

set(name "dabu_cli")
set(src main.c ../lz4.c ../dabu.c)

add_executable(dabu_cli ${src})
