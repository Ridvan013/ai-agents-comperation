cmake_minimum_required(VERSION 3.10)
project(bookstore)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(code
    src/main.cpp
    src/bookstore.cpp
)

set_target_properties(code PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
)

# Place executable at project root as required by OJ
add_custom_command(TARGET code POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:code> ${CMAKE_SOURCE_DIR}/code
)