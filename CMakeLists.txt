cmake_minimum_required(VERSION 3.30)


set(CMAKE_CXX_STANDARD 20)

project(confile)

add_executable(confile src/main.cpp)

target_link_libraries(confile z)