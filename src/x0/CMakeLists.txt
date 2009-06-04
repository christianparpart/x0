#set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(..)
include_directories(${BOOST_INCLUDEDIR})

add_definitions(-Wall -pthread
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DVERSION="${PACKAGE_VERSION}")

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
target_link_libraries(x0d
	boost_system boost_signals
	${LUA_LIBRARIES}
	dl pthread
)

install(TARGETS x0d RUNTIME DESTINATION sbin)
install(FILES x0d.conf DESTINATION etc)
