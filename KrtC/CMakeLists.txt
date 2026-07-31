cmake_minimum_required(VERSION 3.20)
project(placeholder C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

file(GLOB_RECURSE SRC "*.c")
list(FILTER SRC EXCLUDE REGEX ".*crt0_min\\.c$")
list(FILTER SRC EXCLUDE REGEX ".*KrtStartup\\.c$")
list(FILTER SRC EXCLUDE REGEX ".*VmMain\\.c$")
list(FILTER SRC EXCLUDE REGEX ".*Standalone/KrtPrintf\\.c$")
list(FILTER SRC EXCLUDE REGEX ".*/CMakeFiles/.*")
list(FILTER SRC EXCLUDE REGEX ".*IrGenOriginal\\.c.*")
add_executable(krtc ${SRC})

target_include_directories(krtc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../ArkLink/include
)

target_link_libraries(krtc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ArkLink/build/libarklink.a
    m
)