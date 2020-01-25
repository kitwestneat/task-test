cmake_minimum_required(VERSION 3.10)

# set the project name
project(uring-test)

include_directories(include)
set(CMAKE_BUILD_TYPE Debug)

# add the executable
add_executable(uring-test
	src/main.c
)
# add_subdirectory(src)
target_link_libraries(uring-test uring)
