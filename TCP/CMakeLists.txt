cmake_minimum_required(VERSION 3.27)
project(TCP)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "../TCP/src/*.cpp")

add_library(TCP STATIC ${SOURCES})
target_include_directories(TCP PUBLIC inc)

target_compile_definitions(TCP PRIVATE DEBUG)
