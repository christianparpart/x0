AM_CXXFLAGS = \
	-DXOPEN_SOURCE=600 -D_FILE_OFFSET_BITS=64 \
	-D_GNU_SOURCE \
	-D_LARGEFILE_SOURCE -D_LARGE_FILES \
	-Wall -Wextra -std=c++11 \
	$(PLATFORM_CXXFLAGS) \
	$(LIBPCRE_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	-I$(top_srcdir)/src -I$(top_builddir)/src

AM_LDFLAGS =  \
	$(PLATFORM_LDFLAGS) \
	$(LIBPCRE_LIBS) \
	$(OPENSSL_LIBS) \
	-lz -lbzlib

#configure_file(
#    ${CMAKE_CURRENT_SOURCE_DIR}/sysconfig.h.cmake
#    ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.h)

# # test-base
# file(GLOB_RECURSE xzero_base_test_SRC "*-test.cc")
# add_executable(test-base ${xzero_base_test_SRC})
# target_link_libraries(test-base xzero-base gtest gtest_main)

# # pkg-config target
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xzero-base.pc.cmake
#                ${CMAKE_CURRENT_BINARY_DIR}/xzero-base.pc)
# 
# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xzero-base.pc 
#         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

lib_LIBRARIES = libxzero.a

libxzero_a_LIBADD = $(OPENSSL_LDADD)

libxzero_a_SOURCES = \
	AnsiColor.cc \
	Application.cc \
	base64.cc \
	base64url.cc \
	Buffer.cc \
	BufferUtil.cc \
	CivilTime.cc \
	Counter.cc \
	Duration.cc \
	ExceptionHandler.cc \
	HugeBuffer.cc \
	IEEE754.cc \
	ISO8601.cc \
	IdleTimeout.cc \
	JsonWriter.cc \
	MimeTypes.cc \
	MonotonicClock.cc \
	MonotonicTime.cc \
	Random.cc \
	RegExp.cc \
	RuntimeError.cc \
	StackTrace.cc \
	Status.cc \
	StringUtil.cc \
	Tokenizer.cc \
	TokenShaper.cc \
	UnixTime.cc \
	UnixSignals.cc \
	Uri.cc \
	UTF8.cc \
	WallClock.cc \
	inspect.cc \
 	\
	cli/CLI.cc \
	cli/Flags.cc \
 	\
	executor/SafeCall.cc \
	executor/Executor.cc \
	executor/LocalExecutor.cc \
	executor/PosixScheduler.cc \
	executor/ThreadedExecutor.cc \
	executor/ThreadPool.cc \
 	\
	hash/FNV.cc \
 	\
	io/DataChain.cc \
	io/GzipFilter.cc \
	io/File.cc \
	io/FileDescriptor.cc \
	io/FileView.cc \
	io/FileRepository.cc \
	io/Filter.cc \
	io/FileUtil.cc \
	io/LocalFile.cc \
	io/LocalFileRepository.cc \
	io/MemoryFile.cc \
	io/MemoryFileRepository.cc \
	io/MemoryMap.cc \
	io/InputStream.cc \
	io/FileInputStream.cc \
	io/BufferInputStream.cc \
	io/StringInputStream.cc \
	io/OutputStream.cc \
	io/FileOutputStream.cc \
	io/BufferOutputStream.cc \
	io/StringOutputStream.cc \
 	\
	logging/ConsoleLogTarget.cc \
	logging/LogLevel.cc \
	logging/LogTarget.cc \
	logging/Logger.cc \
 	\
	net/ByteArrayEndPoint.cc \
	net/Cidr.cc \
	net/Connection.cc \
	net/ConnectionFactory.cc \
	net/Connector.cc \
	net/DatagramConnector.cc \
	net/DatagramEndPoint.cc \
	net/DnsClient.cc \
	net/EndPoint.cc \
	net/EndPointWriter.cc \
	net/InetAddress.cc \
	net/InetConnector.cc \
	net/InetEndPoint.cc \
	net/LocalConnector.cc \
	net/LocalDatagramConnector.cc \
	net/LocalDatagramEndPoint.cc \
	net/Server.cc \
	net/SslConnector.cc \
	net/SslContext.cc \
	net/SslEndPoint.cc \
	net/UdpClient.cc \
	net/UdpConnector.cc \
	net/UdpEndPoint.cc \
	\
	thread/SignalHandler.cc \
	thread/Wakeup.cc \
	\
	text/IniFile.cc \
	\
	util/BinaryMessageReader.cc \
	util/BinaryMessageWriter.cc \
	util/SimpleRateLimit.cc \
	\
	# HTTP common \
	http/BadMessage.cc \
	http/Cookies.cc \
	http/HeaderField.cc \
	http/HeaderFieldList.cc \
	http/HttpChannel.cc \
	http/HttpConnectionFactory.cc \
	http/HttpDateGenerator.cc \
	http/HttpFileHandler.cc \
	http/HttpListener.cc \
	http/HttpMethod.cc \
	http/HttpOutputCompressor.cc \
	http/HttpRequest.cc \
	http/HttpRequestInfo.cc \
	http/HttpResponse.cc \
	http/HttpService.cc \
	http/HttpStatus.cc \
	http/HttpTransport.cc \
	http/HttpVersion.cc \
	\
	# HTTP transport: mock \
	http/mock/Transport.cc \
	\
	# HTTP transport: fastcgi \
	http/fastcgi/bits.cc \
	http/fastcgi/Connection.cc \
	http/fastcgi/ConnectionFactory.cc \
	http/fastcgi/Generator.cc \
	http/fastcgi/RequestParser.cc \
	http/fastcgi/ResponseParser.cc \
	\
	# HTTP transport: http/1.1 \
	http/http1/Channel.cc \
	http/http1/ConnectionFactory.cc \
	http/http1/Connection.cc \
	http/http1/Generator.cc \
	http/http1/Parser.cc \
	\
	# HTTP transport: http/2 \
	http/http2/Connection.cc \
	http/http2/ConnectionFactory.cc \
	http/http2/ErrorCode.cc \
	http/http2/FrameType.cc \
	http/http2/Generator.cc \
	http/http2/Parser.cc \
	http/http2/SettingParameter.cc \
	http/http2/Settings.cc \
	http/http2/StreamState.cc \
	http/http2/Stream.cc \
	http/hpack/DynamicTable.cc \
	http/hpack/Generator.cc \
	http/hpack/Huffman.cc \
	http/hpack/Parser.cc \
	http/hpack/StaticTable.cc \
	\
	# HTTP client \
	http/client/HttpClient.cc \
	http/client/HttpTransport.cc \
	http/client/Http1Connection.cc \
	http/client/HttpCluster.cc \
	http/client/HttpClusterApiHandler.cc \
	http/client/HttpClusterMember.cc \
	http/client/HttpClusterRequest.cc \
	http/client/HttpClusterScheduler.cc \
	http/client/HttpHealthMonitor.cc
