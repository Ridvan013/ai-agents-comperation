cmake_minimum_required(VERSION 3.10)
project(int2048 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

include_directories(${CMAKE_SOURCE_DIR}/src/include)

# Test / demo executable exercising the int2048 implementation.
add_executable(int2048_test
  src/int2048.cpp
  tests/test_main.cpp
)
