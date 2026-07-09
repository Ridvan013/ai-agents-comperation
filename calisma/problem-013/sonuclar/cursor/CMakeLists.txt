cmake_minimum_required(VERSION 3.10)
project(stlite_map CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# map.hpp includes utility.hpp / exceptions.hpp from src/.
include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/data)

# Build one executable per public test driver. Each test driver has its own
# main() and writes its output (see freopen calls / stdout) for comparison
# against the matching answer.txt.
foreach(test one two three four five)
    add_executable(test_${test} data/${test}/code.cpp)
endforeach()
