cmake_minimum_required(VERSION 3.22.1)
project("amongus_ext")

add_library(amongus_ext SHARED native-lib.cpp)
find_library(log-lib log)
target_link_libraries(amongus_ext ${log-lib})
