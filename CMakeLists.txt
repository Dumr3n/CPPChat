cmake_minimum_required(VERSION 3.20)

project(CPPChat LANGUAGES CXX)

include(CTest)

if (NOT WIN32)
    message(FATAL_ERROR "Windows only, uses Winsock.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(server
    server/main.cpp
    server/ChatServer.cpp
    common/Protocol.cpp
)

add_executable(client
    client/main.cpp
    common/Protocol.cpp
)

target_include_directories(server PRIVATE common server)
target_include_directories(client PRIVATE common client)

target_link_libraries(server PRIVATE ws2_32)
target_link_libraries(client PRIVATE ws2_32)

if (MSVC)
    target_compile_options(server PRIVATE /W4 /permissive-)
    target_compile_options(client PRIVATE /W4 /permissive-)
endif()

if (BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(protocol_tests
        tests/ProtocolTests.cpp
        common/Protocol.cpp
    )
    target_include_directories(protocol_tests PRIVATE common)
    target_link_libraries(protocol_tests PRIVATE GTest::gtest_main ws2_32)

    if (MSVC)
        target_compile_options(protocol_tests PRIVATE /W4 /permissive-)
    endif()

    include(GoogleTest)
    gtest_discover_tests(protocol_tests)
endif()

set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
