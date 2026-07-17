cmake_minimum_required(VERSION 3.10)
project(bookstore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# The judge picks up the `code` binary from the repository root, and it builds
# in-source (`cmake .`), so send the executable there explicitly.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(code
  src/main.cpp
  src/bookstore.cpp
  src/account_system.cpp
  src/book_system.cpp
  src/log_system.cpp
)

target_include_directories(code PRIVATE src)
