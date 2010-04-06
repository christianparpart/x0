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
	io/file.cpp
	io/fd_source.cpp
	io/file_source.cpp
	io/buffer_source.cpp
	io/fd_sink.cpp
	io/file_sink.cpp
	io/connection_sink.cpp
	io/buffer_sink.cpp
	io/filter.cpp
	io/chain_filter.cpp
	io/null_filter.cpp
	io/compress_filter.cpp
	io/chunked_filter.cpp
	io/async_writer.cpp
	io/fileinfo_service.cpp
	io/fileinfo.cpp
)

set(x0_io_API
	io/file.hpp
	io/source.hpp
	io/fd_source.hpp
	io/file_source.hpp
	io/buffer_source.hpp
	io/filter_source.hpp
	io/sink.hpp
	io/fd_sink.hpp
	io/file_sink.hpp
	io/connection_sink.hpp
	io/buffer_sink.hpp
	io/filter.hpp
	io/chain_filter.hpp
	io/compress_filter.hpp
	io/chunked_filter.hpp
	io/pump.hpp
	io/async_writer.hpp
	io/fileinfo.hpp io/fileinfo.ipp
	io/fileinfo_service.hpp
)

set(x0_util_SRC
	process.cpp
	ansi_color.cpp
)

set(x0_util_API
	datetime.hpp
	process.hpp
	ansi_color.hpp
)

set(x0_SRC
	${x0_io_SRC}
	${x0_util_SRC}
	connection.cpp
	context.cpp
	listener.cpp
	logger.cpp
	response.cpp
	request.cpp
	server.cpp
	settings.cpp
	severity.cpp
	ssl_db_cache.cpp
	strutils.cpp
)

set(x0_API
	${x0_io_API}
	${x0_util_API}
	connection.hpp
	context.hpp
	handler.hpp
	event_handler.hpp
	utility.hpp
	header.hpp
	listener.hpp
	local_stream.hpp
	logger.hpp
	request.hpp
	request_parser.hpp
	response.hpp
	server.hpp
	settings.hpp
	severity.hpp
	ssl_db_cache.hpp
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
	${ZLIB_LIBS}
	${BZIP2_LIBS}
	${LIBEV_LIBS}
	${GNUTLS_LIBRARIES}
)

install(FILES ${x0_API} DESTINATION include/x0)
#install(PUBLIC_HEADER DESTINATION include/x0)
