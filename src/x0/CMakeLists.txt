include_directories(..)
add_definitions(-Wall -std=c++0x -pthread -ggdb3 -O0)

link_libraries(dl pthread boost_system stdc++)
INCLUDE(FindPkgConfig)
pkg_check_modules(LUA lua)

add_executable(x0d
	config.cpp
	connection.cpp
	connection_manager.cpp
	listener.cpp
	main.cpp
	mime_types.cpp
	reply.cpp
	request_handler.cpp
	request_parser.cpp
	server.cpp
)
