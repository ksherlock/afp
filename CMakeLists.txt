cmake_minimum_required(VERSION 3.1)
project(afp VERSION 0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)


if (WIN32 OR CYGWIN OR MSYS OR MINGW)
	if (NOT MSVC)
		set(REMAP src/remap_os_error.c)
	endif()
else()
	set(XATTR srx/xattr.c)
endif()



add_library(afp src/finder_info.cpp src/resource_fork.cpp ${XATTR} ${REMAP})

target_include_directories(afp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_include_directories(afp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/afp)
