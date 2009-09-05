#set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(..)
include_directories(${ASIO_INCLUDEDIR})
include_directories(${BOOST_INCLUDEDIR})

SET(COMPILE_DEFINITIONS_Debug -DDEBUG=1)
SET(COMPILE_DEFINITIONS_Release -DRELEASE=1 -DNDEBUG=1)

add_definitions(
	-Dlinux=1
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-DPACKAGE_NAME="${PACKAGE_NAME}"
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DVERSION="${PACKAGE_VERSION}")

add_executable(x0d
	connection.cpp
	fileinfo_service.cpp
	listener.cpp
	logger.cpp
	main.cpp
	process.cpp
	response.cpp
	request.cpp
	server.cpp
	settings.cpp
	severity.cpp
	strutils.cpp
)

set(x0d_API
	connection.hpp
	fileinfo.hpp fileinfo.ipp
	fileinfo_service.hpp
	context.hpp
	datetime.hpp
	handler.hpp
	event_handler.hpp
	utility.hpp
	header.hpp
	listener.hpp
	local_stream.hpp
	logger.hpp
	process.hpp
	request.hpp
	response.hpp
	server.hpp
	settings.hpp
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
add_plugin(alias)
add_plugin(cgi)
add_plugin(debug)
add_plugin(dirlisting)
add_plugin(indexfile)
add_plugin(sendfile)
add_plugin(userdir)
add_plugin(vhost_basic)
add_plugin(vhost_template)

#add_plugin(ssl)
#target_link_libraries(ssl ${GNUTLS_LIBRARIES})
