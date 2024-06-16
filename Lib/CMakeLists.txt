cmake_minimum_required(VERSION 3.2)
project(Lib)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "../Lib/sha256/src/*.c")
file(GLOB HEADERS "../Lib/sha256/inc/*.h")

add_library(Lib STATIC ${SOURCES} ${HEADERS})
target_include_directories(Lib PUBLIC ../Lib/sha256/inc)

target_compile_definitions(Lib PRIVATE DEBUG)
