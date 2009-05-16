include_directories(..)
add_definitions(-Wall -std=c++0x -pthread -ggdb3 -O0)
link_libraries(dl pthread boost_system stdc++)

add_library(x0 STATIC
	config.cpp
	server.cpp
)

#add_library(x0 STATIC
#	server.cpp
#	community.cpp
#	account.cpp
#	identity.cpp
#	group.cpp
#	relationship.cpp
#	guest_book.cpp
#	login_session.cpp
#)
