cmake_minimum_required(VERSION 3.21)

project(imgui-console)

if (PROJECT_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    	
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin-lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin-etc")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED)

    set(FOX_IMGUI_CONSOLE_BUILD_SAMPLES ON CACHE BOOL "")
endif()

option(FOX_IMGUI_CONSOLE_BUILD_SAMPLES "If samples are built." OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)
add_subdirectory("include")
add_library(fox::imgui_console ALIAS imgui_console)

if (FOX_IMGUI_CONSOLE_BUILD_SAMPLES)
	add_subdirectory("sample")
endif()