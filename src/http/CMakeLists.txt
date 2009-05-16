include_directories(..)
add_definitions(-Wall -std=c++0x -pthread -ggdb3 -O0)
link_libraries(dl pthread boost_system)

add_library(http STATIC
	connection.cpp
	connection_manager.cpp
	mime_types.cpp
	reply.cpp
	request_handler.cpp
	request_parser.cpp
	listener.cpp
)
