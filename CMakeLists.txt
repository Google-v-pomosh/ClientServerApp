cmake_minimum_required(VERSION 3.2)
project(all)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(TCP)
add_subdirectory(Client)
add_subdirectory(Client2)
add_subdirectory(Server)
add_subdirectory(SQLite)
#[[add_subdirectory(Lib)]]

#[[include_directories(SQLite/Lib/inc)]]

add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/clean.cmake
)
