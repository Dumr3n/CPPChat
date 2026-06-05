cmake_minimum_required(VERSION 3.20)

project(CPPChat LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "Windows only, uses Winsock.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server
    server/main.cpp
)

add_executable(client
    client/main.cpp
)

target_link_libraries(server PRIVATE ws2_32)
target_link_libraries(client PRIVATE ws2_32)