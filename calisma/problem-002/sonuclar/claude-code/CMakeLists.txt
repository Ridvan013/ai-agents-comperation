cmake_minimum_required(VERSION 3.10)
project(int2048 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The big-integer library.
add_library(int2048 src/int2048.cpp)
target_include_directories(int2048 PUBLIC src/include)

# A small REPL used by the reference test harness (tests/reference_test.py).
add_executable(repl tests/repl.cpp)
target_link_libraries(repl PRIVATE int2048)
