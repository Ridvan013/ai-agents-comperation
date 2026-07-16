cmake_minimum_required(VERSION 3.5)

project (scheme)

if(DEFINED ENV{ONLINE_JUDGE})
    add_definitions(-DONLINE_JUDGE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Remove custom output path settings, use default build directory

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/syntax.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RE.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/expr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/evaluation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Def.cpp
)

add_executable(code ${SOURCES})

# Set C++ standard
set_target_properties(code PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

# Link pthreads: the REPL runs on a thread with an enlarged stack so that deep
# recursion in interpreted programs does not overflow the C++ call stack.
find_package(Threads REQUIRED)
target_link_libraries(code PRIVATE Threads::Threads)

target_compile_options(code
  PRIVATE
    -g
)
