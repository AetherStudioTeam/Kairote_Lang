cmake_minimum_required(VERSION 3.20)
project(placeholder C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

file(GLOB_RECURSE SRC "*.c")
add_executable(lsp ${SRC})