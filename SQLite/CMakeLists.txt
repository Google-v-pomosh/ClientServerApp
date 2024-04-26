cmake_minimum_required(VERSION 3.27)
project(SQLite)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "main.cpp" "Lib/src/*.c")

add_executable(SQLite ${SOURCES}
        Lib/inc/header.h
        Lib/src/sourse.cpp)

target_include_directories(SQLite PUBLIC Lib/inc)

