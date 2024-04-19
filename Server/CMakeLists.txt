cmake_minimum_required(VERSION 3.27)
project(Server)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "main.cpp" "../TCP/src/*.cpp" "../Server/TCP/src/*.cpp")

add_executable(Server ${SOURCES})

target_include_directories(Server PRIVATE ../SQLite/Windows/inc)

if(WIN32)
    target_link_libraries(Server PRIVATE wsock32 ws2_32 -lsqlite3)
    target_link_directories(Server PRIVATE ../SQLite/Windows/lib/x64)
endif()

target_include_directories(Server PRIVATE ../TCP/inc)
target_include_directories(Server PRIVATE ../Server/TCP/inc)
