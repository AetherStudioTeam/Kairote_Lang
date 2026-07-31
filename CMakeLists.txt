cmake_minimum_required(VERSION 3.20)
project(KairoteAll C CXX)

include_directories(/usr/include /usr/include/x86_64-linux-gnu)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(ArkLink)
add_subdirectory(KrtC)
add_subdirectory(lsp)