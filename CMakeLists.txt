cmake_minimum_required(VERSION 3.8)
project(joy)

add_executable(joy main.cpp parser.cpp passes.cpp codegen_c.cpp)
target_compile_features(joy PUBLIC cxx_std_17)
