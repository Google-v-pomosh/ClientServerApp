cmake_minimum_required(VERSION 3.27)
project(SQLite)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "main.cpp" "Windows/src/*.c")

add_executable(SQLite ${SOURCES}
        Windows/inc/header.h
        Windows/src/sourse.cpp)

target_include_directories(SQLite PUBLIC Windows/inc)

