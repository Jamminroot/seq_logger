cmake_minimum_required(VERSION 3.16)
project(seq_example_usage)

set(CMAKE_CXX_STANDARD 17)

#set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fno-pic -fPIE -ftemplate-backtrace-limit=1")
#set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

include_directories(src)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
add_executable(seq_example_usage
        example.cpp
        additional_unit.cpp additional_unit.h)