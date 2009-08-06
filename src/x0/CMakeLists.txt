#set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(..)
include_directories(${BOOST_INCLUDEDIR})

SET(COMPILE_DEFINITIONS_Debug -DDEBUG=1)
SET(COMPILE_DEFINITIONS_Release -DRELEASE=1 -DNDEBUG=1)

IF(${CMAKE_BUILD_TYPE} EQUAL Release)
	add_definitions(-DX0_RELEASE -DNDEBUG=1)
ELSE()
	add_definitions(-DX0_DEBUG=1)
ENDIF()

add_definitions(
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-DPACKAGE_NAME="${PACKAGE_NAME}"
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DVERSION="${PACKAGE_VERSION}")

add_executable(x0d
	config.cpp
	connection.cpp
	fileinfo_service.cpp
	io_service_pool.cpp
	listener.cpp
	logger.cpp
	main.cpp
	process.cpp
	response.cpp
	request.cpp
	server.cpp
	severity.cpp
	strutils.cpp
)

set(x0d_API
	config.hpp
	connection.hpp
	fileinfo.hpp fileinfo.ipp
	fileinfo_service.hpp
	io_service_pool.hpp
	context.hpp
	handler.hpp
	header.hpp
	listener.hpp
	local_stream.hpp
	logger.hpp
	process.hpp
	request.hpp
	response.hpp
	server.hpp
	severity.hpp
	strutils.hpp strutils.tcc
	types.hpp
	defines.hpp
	api.hpp
	sysconfig.h
)

#	function.hpp
#	trie.hpp
#	ternary_search.hpp

install(FILES ${x0d_API} DESTINATION include/x0)

target_link_libraries(x0d
	boost_system boost_signals boost_thread
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
add_plugin(cgi)
add_plugin(debug)
add_plugin(dirlisting)
add_plugin(indexfile)
add_plugin(sendfile)
add_plugin(userdir)
add_plugin(vhost)
