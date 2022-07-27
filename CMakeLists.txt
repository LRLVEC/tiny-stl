cmake_minimum_required(VERSION 3.18)
project(test
	LANGUAGES CXX
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TINY_STL_BUILD_TESTS "Build tiny-stl test programs" ON)

if(TINY_STL_BUILD_TESTS)
	add_subdirectory(tests)
endif()