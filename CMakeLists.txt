cmake_minimum_required(VERSION 4.1)
project(GameOfLifeDX12)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_subdirectory(3rdparty/glfw)
add_subdirectory(src/Application)