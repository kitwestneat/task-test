cmake_minimum_required(VERSION 3.10)

# set the project name
project(uring-test)

include_directories(include)
set(CMAKE_BUILD_TYPE Debug)

set(libsrc 
	src/task.c
	src/resource.c
	src/tcp_resource.c
	src/resource_pool.c
	src/disk.c
	src/tcp.c
	src/event_svc.c
	include/bitmap.h
	include/log.h
	include/disk.h
	include/event_svc.h
	include/tcp.h
	include/task.h
	include/resource.h
)

macro(TestApp name)
	add_executable(${name}
		tests/${name}.c
		${libsrc}
	)
	target_link_libraries(${name} uring)
endmacro()

testapp(tcp-test)
testapp(echo-test)
testapp(disk-test)
testapp(chat-test)
testapp(stride-cli-test)
testapp(stride-srv-test)