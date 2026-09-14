cmake_minimum_required(VERSION 3.16)
project(helion_native LANGUAGES CXX)

option(HELION_BUILD_SERVER "Build the headless server" ON)
option(HELION_BUILD_CLIENT "Build the SDL2/OpenGL client" ON)
option(HELION_BUILD_TESTS "Build native protocol tests" ON)

if(NOT HELION_BUILD_SERVER AND NOT HELION_BUILD_CLIENT AND NOT HELION_BUILD_TESTS)
  message(FATAL_ERROR "Enable at least one Helion build option")
endif()

if(HELION_BUILD_TESTS)
  enable_testing()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(native)
