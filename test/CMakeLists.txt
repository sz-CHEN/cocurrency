cmake_minimum_required(VERSION 2.8.12)
project(Test)

include_directories(../include)
link_libraries(cocurrency)
add_executable(Test test.c)
if(WIN32)
else()
target_link_libraries(Test PUBLIC m)
endif()

