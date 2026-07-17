cmake_minimum_required(VERSION 3.10)
project(ticket_system)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(code src/main.cpp)
target_include_directories(code PRIVATE src)

if(MSVC)
  target_compile_options(code PRIVATE /O2)
else()
  target_compile_options(code PRIVATE -O2 -Wall)
endif()
