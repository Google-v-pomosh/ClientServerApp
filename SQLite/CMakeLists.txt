cmake_minimum_required(VERSION 3.27)
project(SQLite)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "main.c" "C:/LionProjects/SC/SQLite/Windows/src")

add_library(SQLite STATIC ${SOURCES})

target_include_directories(SQLite PUBLIC Windows/inc)

if (WIN32)
    target_link_libraries(SQLite PRIVATE wsock32 ws2_32)

    target_link_directories(SQLite PRIVATE Windows/lib/x64)
    target_link_libraries(SQLite PRIVATE -l sqlite3)
endif()
