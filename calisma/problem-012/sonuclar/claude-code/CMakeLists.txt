include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_executable(linked_hashmap_one ${CMAKE_CURRENT_SOURCE_DIR}/data/testone/1.cpp)
add_executable(linked_hashmap_two ${CMAKE_CURRENT_SOURCE_DIR}/data/testtwo/3.cpp)
add_executable(linked_hashmap_three ${CMAKE_CURRENT_SOURCE_DIR}/data/testthree/5.cpp)
add_executable(linked_hashmap_four ${CMAKE_CURRENT_SOURCE_DIR}/data/testfour/7.cpp)
add_executable(linked_hashmap_five ${CMAKE_CURRENT_SOURCE_DIR}/data/testfive/9.cpp)
add_executable(linked_hashmap_six ${CMAKE_CURRENT_SOURCE_DIR}/data/testsix/11.cpp)
add_test(NAME linked_hashmap_one COMMAND sh -c "${CMAKE_CURRENT_BINARY_DIR}/linked_hashmap_one >/tmp/one_out.txt\
        && diff -u ${CMAKE_CURRENT_SOURCE_DIR}/data/testone/1.ans /tmp/one_out.txt>/tmp/one_diff.txt")
add_test(NAME linked_hashmap_two COMMAND sh -c "${CMAKE_CURRENT_BINARY_DIR}/linked_hashmap_two >/tmp/two_out.txt\
        && diff -u ${CMAKE_CURRENT_SOURCE_DIR}/data/testtwo/3.ans /tmp/two_out.txt>/tmp/two_diff.txt")
add_test(NAME linked_hashmap_three COMMAND sh -c "${CMAKE_CURRENT_BINARY_DIR}/linked_hashmap_three >/tmp/three_out.txt\
        && diff -u ${CMAKE_CURRENT_SOURCE_DIR}/data/testthree/5.ans /tmp/three_out.txt>/tmp/three_diff.txt")
add_test(NAME linked_hashmap_four COMMAND sh -c "${CMAKE_CURRENT_BINARY_DIR}/linked_hashmap_four >/tmp/four_out.txt\
        && diff -u ${CMAKE_CURRENT_SOURCE_DIR}/data/testfour/7.ans /tmp/four_out.txt>/tmp/four_diff.txt")
add_test(NAME linked_hashmap_five COMMAND sh -c "${CMAKE_CURRENT_BINARY_DIR}/linked_hashmap_five >/tmp/five_out.txt\
        && diff -u ${CMAKE_CURRENT_SOURCE_DIR}/data/testfive/9.ans /tmp/five_out.txt>/tmp/five_diff.txt")
add_test(NAME linked_hashmap_six COMMAND sh -c "${CMAKE_CURRENT_BINARY_DIR}/linked_hashmap_six >/tmp/six_out.txt\
        && diff -u ${CMAKE_CURRENT_SOURCE_DIR}/data/testsix/11.ans /tmp/six_out.txt>/tmp/six_diff.txt")
