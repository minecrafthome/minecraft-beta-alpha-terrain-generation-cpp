cmake_minimum_required(VERSION 3.10)
project(HEROBRINE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -Wall -pedantic -ggdb3")

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

if (LINUX AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(STATUS ">>> ${CMAKE_SYSTEM_NAME}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    link_libraries("-fsanitize=address")
    add_compile_options("-fsanitize=address")
endif ()

add_subdirectory(src)