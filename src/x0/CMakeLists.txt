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
	io/BufferSink.cpp
	io/BufferSource.cpp
	io/ChainFilter.cpp
	io/ChunkedEncoder.cpp
	io/CompositeSource.cpp
	io/CompressFilter.cpp
	io/SocketSink.cpp
	io/SystemSink.cpp
	io/SystemSource.cpp
	io/FileSink.cpp
	io/FileSource.cpp
	io/FileInfo.cpp
	io/FileInfoService.cpp
	io/Filter.cpp
	io/FilterSource.cpp
	io/NullFilter.cpp
)

set(x0_io_API
	io/BufferSink.h
	io/BufferSource.h
	io/ChainFilter.h
	io/ChunkedEncoder.h
	io/CompositeSource.h
	io/CompressFilter.h
	io/SocketSink.h
	io/SystemSink.h
	io/SystemSource.h
	io/FileSink.h
	io/FileSource.h
	io/FileInfo.h io/FileInfo.cc
	io/FileInfoService.h
	io/Filter.h
	io/FilterSource.h
	io/NullFilter.h
	io/pump.hpp
	io/Sink.h
	io/Source.h
	io/SourceVisitor.h
)

set(x0_http_API
	http/HttpConnection.h
	http/HttpContext.h
	http/HttpCore.h
	http/HttpError.h
	http/HttpHeader.h
	http/HttpListener.h
	http/HttpPlugin.h
	http/HttpRangeDef.h http/HttpRangeDef.cc
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
	http/HttpRequest.cpp
	http/HttpResponse.cpp
	http/HttpServer.cpp
)

set(x0_SRC
	AnsiColor.cpp
	Error.cpp
	Library.cpp
	Logger.cpp
	Process.cpp
	Url.cpp
	WebClient.cpp
	Scope.cpp
	Settings.cpp
	Severity.cpp
	Socket.cpp
	SocketDriver.cpp
	StackTrace.cpp
	StringError.cpp
	strutils.cpp
	gai_error.cpp
	${x0_io_SRC}
	${x0_http_SRC}
)

set(x0_API
	AnsiColor.h
	Api.h
	Buffer.h
	BufferRef.h
	Cache.h Cache.cc
	DateTime.h
	Defines.h
	Error.h
	gai_error.h
	function.hpp
	Library.h
	LocalStream.h
	Logger.h
	Process.h
	Property.h
	Scope.h
	Settings.h Settings.cc
	Severity.h
	Socket.h
	SocketDriver.h
	StackTrace.h
	StringError.h
	Signal.h
	strutils.h strutils.cc
	Url.h
	TernarySearch.hpp
	Trie.h
	Types.h
	Utility.h
	WebClient.h
)

#	sysconfig.h
#	function.h
#	Trie.h
#	TernarySearch.h

add_library(x0 SHARED ${x0_SRC})
install(TARGETS x0 DESTINATION ${LIBDIR})

target_link_libraries(x0
	dl pthread
	${FLOW_LIBS}
	${LUA_LIBRARIES}
	${ZLIB_LIBS}
	${BZIP2_LIBS}
	${LIBEV_LIBS}
)

install(FILES ${x0_API} DESTINATION ${INCLUDEDIR}/x0)
install(FILES ${x0_io_API} DESTINATION ${INCLUDEDIR}/x0/io)
install(FILES ${x0_http_API} DESTINATION ${INCLUDEDIR}/x0/io)
#install(PUBLIC_HEADER DESTINATION include/x0)
