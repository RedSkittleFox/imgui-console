cmake_minimum_required(VERSION 3.21)

add_library(
    imgui_console
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/fox/imgui_console.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/fox/imgui_console.cpp"
)

target_sources(
    imgui_console
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/fox/imgui_console.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/fox/imgui_console.cpp"
)

target_include_directories(
    imgui_console
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)