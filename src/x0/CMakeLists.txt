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
	io/async_writer.cpp
	io/buffer_sink.cpp
	io/buffer_source.cpp
	io/chain_filter.cpp
	io/chunked_filter.cpp
	io/chunked_decoder.cpp
	io/compress_filter.cpp
	io/connection_sink.cpp
	io/file.cpp
	io/fd_sink.cpp
	io/fd_source.cpp
	io/file_sink.cpp
	io/file_source.cpp
	io/fileinfo.cpp
	io/fileinfo_service.cpp
	io/filter.cpp
	io/null_filter.cpp
)

set(x0_io_API
	io/async_writer.hpp
	io/buffer_sink.hpp
	io/buffer_source.hpp
	io/chain_filter.hpp
	io/chunked_filter.hpp
	io/chunked_decoder.hpp
	io/composite_source.hpp
	io/compress_filter.hpp
	io/connection_sink.hpp
	io/fd_sink.hpp
	io/fd_source.hpp
	io/file.hpp
	io/file_sink.hpp
	io/file_source.hpp
	io/fileinfo.hpp io/fileinfo.ipp
	io/fileinfo_service.hpp
	io/filter.hpp
	io/filter_source.hpp
	io/null_filter.hpp
	io/pump.hpp
	io/sink.hpp
	io/source.hpp
	io/source_visitor.hpp
	io/uppercase_filter.hpp
)

set(x0_SRC
	${x0_io_SRC}
	connection.cpp
	context.cpp
	listener.cpp
	logger.cpp
	response.cpp
	web_client.cpp
	request.cpp
	server.cpp
	settings.cpp
	severity.cpp
	ssl_db_cache.cpp
	strutils.cpp
	url.cpp
	process.cpp
	ansi_color.cpp
)

set(x0_API
	ansi_color.hpp
	api.hpp
	buffer.hpp
	buffer_ref.hpp
	cache.hpp cache.ipp
	connection.hpp
	context.hpp
	datetime.hpp
	defines.hpp
	event_handler.hpp
	function.hpp
	handler.hpp
	header.hpp
	listener.hpp
	local_stream.hpp
	logger.hpp
	plugin.hpp
	process.hpp
	property.hpp
	range_def.hpp range_def.ipp
	request.hpp
	request_handler.hpp
	request_parser.hpp
	response.hpp
	response_parser.hpp
	server.hpp
	settings.hpp settings.ipp
	severity.hpp
	signal.hpp
	ssl_db_cache.hpp
	strutils.hpp strutils.ipp
	url.hpp
	sysconfig.h
	ternary_search.hpp
	trie.hpp
	types.hpp
	utility.hpp
	web_client.hpp
)

#	function.hpp
#	trie.hpp
#	ternary_search.hpp

add_library(x0 SHARED ${x0_SRC})
install(TARGETS x0 DESTINATION ${LIBDIR})

target_link_libraries(x0
	boost_system boost_signals boost_thread
	${LUA_LIBRARIES}
	dl pthread
	${ZLIB_LIBS}
	${BZIP2_LIBS}
	${LIBEV_LIBS}
	${GNUTLS_LIBRARIES}
)

install(FILES ${x0_API} DESTINATION ${INCLUDEDIR}/x0)
install(FILES ${x0_io_API} DESTINATION ${INCLUDEDIR}/x0/io)
#install(PUBLIC_HEADER DESTINATION include/x0)
