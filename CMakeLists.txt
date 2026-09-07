cmake_minimum_required(VERSION 3.24)

project(Manimate
    VERSION 0.2.0
    DESCRIPTION "Visual editor for Manim"
    LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(MANIMATE_BUILD_TESTS "Build the test suite" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Multimedia)
qt_standard_project_setup()

add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src/app)

if(MANIMATE_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()
