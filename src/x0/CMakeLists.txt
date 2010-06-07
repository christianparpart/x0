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
	io/AsyncWriter.cpp
	io/BufferSink.cpp
	io/BufferSource.cpp
	io/ChainFilter.cpp
	io/ChunkedEncoder.cpp
	io/CompressFilter.cpp
	io/ConnectionSink.cpp
	io/File.cpp
	io/SystemSink.cpp
	io/SystemSource.cpp
	io/FileSink.cpp
	io/FileSource.cpp
	io/FileInfo.cpp
	io/FileInfoService.cpp
	io/Filter.cpp
	io/NullFilter.cpp
)

set(x0_io_API
	io/AsyncWriter.h
	io/BufferSink.h
	io/BufferSource.h
	io/ChainFilter.h
	io/ChunkedEncoder.h
	io/CompositeSource.h
	io/CompressFilter.h
	io/ConnectionSink.h
	io/SystemSink.h
	io/SystemSource.h
	io/File.h
	io/FileSink.h
	io/FileSource.h
	io/FileInfo.h io/FileInfo.cc
	io/FileInfoService.h
	io/Filter.h
	io/FilterSource.h
	io/NullFilter.h
	io/pump.h
	io/Sink.h
	io/Source.h
	io/SourceVisitor.h
)

set(x0_http_API
	http/HttpConnection.h
	http/HttpContext.h
	http/HttpCore.h
	http/HttpError.h
	http/HttpListener.h
	http/HttpRequest.h
	http/HttpResponse.h
	http/HttpMessageProcessor.h
	http/HttpServer.h
)

set(x0_http_SRC
	http/HttpConnection.cpp
	http/HttpCore.cpp
	http/HttpError.cpp
	http/HttpListener.cpp
	http/HttpMessageProcessor.cpp
	http/HttpPlugin.cpp
	http/HttpResponse.cpp
	http/HttpRequest.cpp
	http/HttpServer.cpp
)

set(x0_SRC
	AnsiColor.cpp
	Library.cpp
	Logger.cpp
	Process.cpp
	Url.cpp
	WebClient.cpp
	Scope.cpp
	Settings.cpp
	Severity.cpp
	ssl_db_cache.cpp # should go into io/
	strutils.cpp
	gai_error.cpp
	${x0_io_SRC}
	${x0_http_SRC}
)

set(x0_API
	ansi_color.h
	api.h
	buffer.h
	buffer_ref.h
	cache.h cache.cc
	connection.h
	datetime.h
	defines.h
	event_handler.h
	gai_error.h
	function.h
	handler.h
	header.h
	library.h
	local_stream.h
	logger.h
	plugin.h
	process.h
	property.h
	range_def.h range_def.cc
	scope.h
	settings.h settings.cc
	severity.h
	signal.h
	ssl_db_cache.h
	strutils.h strutils.cc
	url.h
	sysconfig.h
	ternary_search.h
	trie.h
	types.h
	utility.h
	web_client.h
)

#	function.h
#	trie.h
#	ternary_search.h

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
install(FILES ${x0_http_API} DESTINATION ${INCLUDEDIR}/x0/io)
#install(PUBLIC_HEADER DESTINATION include/x0)
