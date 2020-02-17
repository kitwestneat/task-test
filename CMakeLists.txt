cmake_minimum_required(VERSION 3.10)

# set the project name
project(uring-test)

include_directories(include)
set(CMAKE_BUILD_TYPE Debug)

add_executable(tcp-test
	tests/tcp-test.c
	src/tcp.c
    include/log.h
	include/tcp.h
)
target_link_libraries(tcp-test uring)

add_executable(echo-test
	tests/echo-test.c
	src/resource.c
	src/resource_pool.c
	src/tcp.c
    include/log.h
	include/tcp.h
	include/resource.h
)
target_link_libraries(echo-test uring)
