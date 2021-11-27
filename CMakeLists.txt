cmake_minimum_required(VERSION 3.16)
project(seq_example_usage)

set(CMAKE_CXX_STANDARD 17)

include_directories(src)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
add_executable(seq_example_usage example.cpp)