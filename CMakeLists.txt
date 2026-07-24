cmake_minimum_required(VERSION 3.8)
project(joy)

add_executable(joy main.cpp parser.cpp passes.cpp)
set_target_properties(joy PROPERTIES CXX_STANDARD 14)
