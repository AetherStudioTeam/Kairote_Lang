cmake_minimum_required(VERSION 3.20)
project(placeholder C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

file(GLOB_RECURSE SRC "*.c")
list(FILTER SRC EXCLUDE REGEX ".*Main\\.c$")
list(FILTER SRC EXCLUDE REGEX ".*CMakeCCompilerId\\.c$")
add_library(arklink STATIC ${SRC})

target_compile_options(arklink PRIVATE
    -Wall
    -Wextra
    -Werror=unused-variable
    -Werror=unused-function
    -Werror=unused-parameter
    -Werror=unused-but-set-variable
    -Werror=implicit-function-declaration
)

target_compile_definitions(arklink PRIVATE ARK_DEBUG)

target_include_directories(arklink PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)