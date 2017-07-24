cmake_minimum_required(VERSION 3.1)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)


add_library(afp src/finder_info.cpp src/resource_fork.cpp src/xattr.c)

target_include_directories(afp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_include_directories(afp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/afp)
