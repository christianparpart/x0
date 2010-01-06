#set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${ASIO_INCLUDEDIR})
include_directories(${BOOST_INCLUDEDIR})

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

set(x0_io_SRC
	fd_source.cpp
	file_source.cpp
	buffer_source.cpp
	fd_sink.cpp
	file_sink.cpp
	buffer_sink.cpp
	filter.cpp
	chain_filter.cpp
	null_filter.cpp
)

set(x0_io_API
	source.hpp
	fd_source.hpp
	file_source.hpp
	buffer_source.hpp
	sink.hpp
	fd_sink.hpp
	file_sink.hpp
	buffer_sink.hpp
	filter.hpp
	chain_filter.hpp
	pump.hpp
)

set(x0_SRC
	${x0_io_SRC}
	connection.cpp
	fileinfo_service.cpp
	listener.cpp
	logger.cpp
	response.cpp
	request.cpp
	server.cpp
	process.cpp
	settings.cpp
	severity.cpp
	strutils.cpp
)

set(x0_API
	${x0_io_API}
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

add_library(x0 SHARED ${x0_SRC})

target_link_libraries(x0
	boost_system boost_signals boost_thread
	${LUA_LIBRARIES}
	dl pthread
)

install(FILES ${x0_API} DESTINATION include/x0)
#install(PUBLIC_HEADER DESTINATION include/x0)
