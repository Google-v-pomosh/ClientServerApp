cmake_minimum_required(VERSION 3.27)
project(Client2)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES   "main.cpp"
                    "../Client/TCP/src/*.cpp"
                    "../TCP/src/*.cpp"
                    "../Lib/sha256/src/*.c")


add_executable(Client2 ${SOURCES})

if(WIN32)
    target_link_libraries(Client2 wsock32 ws2_32 -liphlpapi)
endif()

target_include_directories(Client2 PRIVATE ../TCP/inc)
target_include_directories(Client2 PRIVATE ../Client/TCP/inc)

target_compile_definitions(Client2 PRIVATE DEBUG)
