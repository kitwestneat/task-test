cmake_minimum_required(VERSION 3.10)

# set the project name
project(uring-test)

include_directories(include)
set(CMAKE_BUILD_TYPE Debug)

# target_link_libraries(uring-test uring)

add_executable(ra-test
	tests/ra-test.c
	src/resource.c
	src/resource_pool.c
	src/foobar.c
	include/resource.h
	include/log.h
	include/foobar.h
)

add_executable(foo-test
	tests/foo-test.c
	src/task.c
	src/resource.c
	src/resource_pool.c
	src/foobar.c
	include/task.h
	include/resource.h
	include/log.h
	include/foobar.h
)

add_executable(taskception-test
	tests/taskception-test.c
	src/task.c
	src/resource.c
	src/resource_pool.c
	src/foobar.c
	include/task.h
	include/resource.h
	include/log.h
	include/foobar.h
)

add_executable(foobar-test
	tests/foobar-test.c
	src/task.c
	src/resource.c
	src/resource_pool.c
	src/foobar.c
	include/task.h
	include/resource.h
	include/log.h
	include/foobar.h
)

add_executable(bigfoo-test
	tests/bigfoo-test.c
	src/task.c
	src/resource.c
	src/resource_pool.c
	src/foobar.c
	include/task.h
	include/resource.h
	include/log.h
	include/foobar.h
)
