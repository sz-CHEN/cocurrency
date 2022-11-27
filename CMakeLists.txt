cmake_minimum_required(VERSION 2.8.12)
project(Cocurrency)

include(conf.cmake)

include_directories(./include)

add_library(cstdthreads src/cstdthreads.c src/cstdtime.c def/cstdthreads.def)

if(WIN32)
#target_link_libraries(cstdthreads PUBLIC MinCore)
else()
target_link_libraries(cstdthreads PUBLIC pthread)
endif()

add_library(cocurrency src/cfuture.c def/coccurency.def)
target_link_libraries(cocurrency cstdthreads)

add_subdirectory(test)
