cmake_minimum_required(VERSION 2.6)
project(malloc_stats)

add_executable(malloc_stats main.cpp)

install(TARGETS malloc_stats RUNTIME DESTINATION bin)
