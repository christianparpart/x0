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
	severity.cpp
	strutils.cpp
)

set(x0d_API
	config.hpp
	connection.hpp
	connection_manager.hpp
	context.hpp
	handler.hpp
	header.hpp
	listener.hpp
	logger.hpp
	request.hpp
	request_parser.hpp
	response.hpp
	server.hpp
	strutils.hpp strutils.tcc
	types.hpp
)

#	function.hpp
#	trie.hpp
#	ternary_search.hpp

install(FILES ${x0d_API} DESTINATION include/x0)

target_link_libraries(x0d
	boost_system boost_signals
	${LUA_LIBRARIES}
	dl pthread
)

install(TARGETS x0d
	RUNTIME DESTINATION sbin
	PUBLIC_HEADER DESTINATION include/x0
)

install(FILES x0d.conf-dist DESTINATION etc)

macro(add_plugin ModuleName)
	add_library(${ModuleName} MODULE mod_${ModuleName}.cpp)
	set_target_properties(${ModuleName} PROPERTIES PREFIX "")
	install(TARGETS ${ModuleName} DESTINATION lib/x0)
endmacro()

add_plugin(accesslog)
add_plugin(dirlisting)
add_plugin(indexfile)
add_plugin(sendfile)
add_plugin(vhost)
