cmake_minimum_required(VERSION 3.10)
project(example)

set(CMAKE_CXX_STANDARD 11)

add_executable(os_hw1 smash.cpp Commands.cpp signals.cpp)