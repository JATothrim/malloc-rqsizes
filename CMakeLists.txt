cmake_minimum_required(VERSION 3.0)
project(malloc_stats C)

add_library(malloc_stats SHARED malloc_wrap.c)
target_compile_options(malloc_stats PUBLIC -std=c11 -O0 -Wall -z defs)
target_link_libraries(malloc_stats pthread dl)

add_executable(mallocwrap wraptest.c)
target_link_libraries(mallocwrap pthread)

add_executable(format_stats readstats.c)

install(TARGETS malloc_stats LIBRARY DESTINATION lib)
