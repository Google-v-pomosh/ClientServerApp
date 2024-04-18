cmake_minimum_required(VERSION 3.27)
project(Client)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES   "main.cpp"
                    "TCP/src/*.cpp"
                    "../TCP/src/*.cpp")


add_executable(Client ${SOURCES})

add_definitions(-D_UNICODE)

if(WIN32)
    target_link_libraries(Client wsock32 ws2_32 -liphlpapi)
endif()

target_include_directories(Client PRIVATE ../TCP/inc)
target_include_directories(Client PRIVATE ../Client/TCP/inc)


target_compile_definitions(Client PRIVATE DEBUG)
