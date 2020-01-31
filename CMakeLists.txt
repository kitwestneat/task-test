cmake_minimum_required(VERSION 3.10)

# set the project name
project(uring-test)

include_directories(include)
set(CMAKE_BUILD_TYPE Debug)

# add the executable
add_executable(uring-test
	src/main.c
	src/task.c
	src/resource.c
	include/task.h
	include/resource.h
)
# add_subdirectory(src)
target_link_libraries(uring-test uring)

add_executable(ra-test
	tests/ra-test.c
	src/resource.c
	include/resource.h
)
