cmake_minimum_required(VERSION 3.27)
project(Server)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES   "main.cpp"
                    "../TCP/src/*.cpp"
                    "../Server/TCP/src/*.cpp"
                    "../SQLite/Lib/src/sqlite3.c"
                    "../Lib/sha256/src/sha256.c")

add_executable(Server ${SOURCES})

if(WIN32)
    target_link_libraries(Server PRIVATE wsock32 ws2_32)
endif()

target_include_directories(Server PRIVATE ../SQLite/Lib/inc)
target_include_directories(Server PRIVATE ../TCP/inc)
target_include_directories(Server PRIVATE ../Server/TCP/inc)
