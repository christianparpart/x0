include_directories(..)
add_definitions(-Wall -pthread -ggdb3 -O0)

INCLUDE(FindPkgConfig)
pkg_check_modules(LUA lua)

link_libraries(dl pthread boost_system boost_signals)

add_executable(x0d
	config.cpp
	connection.cpp
	connection_manager.cpp
	listener.cpp
	logger.cpp
	main.cpp
	response.cpp
	request_parser.cpp
	request.cpp
	server.cpp
	strutils.cpp
	mod_accesslog.cpp
	mod_dirlisting.cpp
	mod_indexfile.cpp
	mod_sendfile.cpp
	mod_vhost.cpp
)
