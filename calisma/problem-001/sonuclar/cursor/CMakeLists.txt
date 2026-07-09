cmake_minimum_required(VERSION 3.01)
project(A-plus-B)

set(CMAKE_CXX_STANDARD 14)

include_directories(${project_source_dir}/)

add_executable(code solution.cpp)