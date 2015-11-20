cmake_minimum_required(VERSION 2.8)

project(xzero-base CXX)
set(XZERO_BASE_VERSION "0.11.0-dev")

include(XzeroCommon)
include(FindPkgConfig)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CMakeDetermineCCompiler)
include(FindDoxygen)

include(FindOpenSSL)
if(NOT OPENSSL_FOUND)
  # message(FATAL_ERROR "OpenSSL not found ;-(")
else()
  message(STATUS "OpenSSL found ;-)")
endif()

CHECK_INCLUDE_FILES(sys/sendfile.h HAVE_SYS_SENDFILE_H)
CHECK_FUNCTION_EXISTS(sendfile HAVE_SENDFILE)
CHECK_FUNCTION_EXISTS(posix_fadvise HAVE_POSIX_FADVISE)
CHECK_FUNCTION_EXISTS(readahead HAVE_READAHEAD)
CHECK_FUNCTION_EXISTS(pread HAVE_PREAD)

CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILES(sys/inotify.h HAVE_SYS_INOTIFY_H)
CHECK_INCLUDE_FILES(sys/resource.h HAVE_SYS_RESOURCE_H)
CHECK_INCLUDE_FILES(sys/mman.h HAVE_SYS_MMAN_H)
CHECK_INCLUDE_FILES(sys/limits.h HAVE_SYS_LIMITS_H)
CHECK_INCLUDE_FILES(pwd.h HAVE_PWD_H)
CHECK_INCLUDE_FILES(syslog.h HAVE_SYSLOG_H)
CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
CHECK_INCLUDE_FILES(execinfo.h HAVE_EXECINFO_H)
CHECK_INCLUDE_FILES(uuid/uuid.h HAVE_UUID_UUID_H)

CHECK_FUNCTION_EXISTS(nanosleep HAVE_NANOSLEEP)
CHECK_FUNCTION_EXISTS(daemon HAVE_DAEMON)
CHECK_FUNCTION_EXISTS(sysconf HAVE_SYSCONF)
CHECK_FUNCTION_EXISTS(pathconf HAVE_PATHCONF)
CHECK_FUNCTION_EXISTS(accept4 HAVE_ACCEPT4)
CHECK_FUNCTION_EXISTS(pipe2 HAVE_PIPE2)
CHECK_FUNCTION_EXISTS(dup2 HAVE_DUP2)
CHECK_FUNCTION_EXISTS(dladdr HAVE_DLADDR)
CHECK_FUNCTION_EXISTS(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS(backtrace HAVE_BACKTRACE)
CHECK_FUNCTION_EXISTS(clock_gettime HAVE_CLOCK_GETTIME)

CHECK_INCLUDE_FILES(pthread.h HAVE_PTHREAD_H)
CHECK_LIBRARY_EXISTS(pthread pthread_setname_np "" HAVE_PTHREAD_SETNAME_NP)
CHECK_LIBRARY_EXISTS(pthread pthread_setaffinity_np "" HAVE_PTHREAD_SETAFFINITY_NP)

CHECK_INCLUDE_FILES(sys/inotify.h HAVE_SYS_INOTIFY_H)
if(HAVE_SYS_INOTIFY_H)
  CHECK_FUNCTION_EXISTS(inotify_init1 HAVE_INOTIFY_INIT1)
endif(HAVE_SYS_INOTIFY_H)

CHECK_INCLUDE_FILES(zlib.h HAVE_ZLIB_H)
if(HAVE_ZLIB_H)
  CHECK_LIBRARY_EXISTS(z deflate "" HAVE_LIBZ)
  set(ZLIB_LIBRARIES z)
endif(HAVE_ZLIB_H)

CHECK_INCLUDE_FILES(bzlib.h HAVE_BZLIB_H)
if(HAVE_BZLIB_H)
  CHECK_LIBRARY_EXISTS(bz2 BZ2_bzCompressInit "" HAVE_LIBBZ2)
  set(BZIP2_LIBRARIES bz2)
endif(HAVE_BZLIB_H)

# PCRE
option(ENABLE_PCRE "With PCRE support [default: on]" ON)
if(ENABLE_PCRE)
  pkg_check_modules(PCRE REQUIRED libpcre>=7.0)
else()
  pkg_check_modules(PCRE libpcre>=7.0)
endif()

# required for shm_open/shm_unlink
if(APPLE)
  set(RT_LIBRARIES)
else()
  set(RT_LIBRARIES rt)
endif()

set(xzero_base_SRC
  AnsiColor.cc
  Application.cc
  Base64.cc
  Buffer.cc
  BufferUtil.cc
  CivilTime.cc
  Counter.cc
  Duration.cc
  ExceptionHandler.cc
  IEEE754.cc
  ISO8601.cc
  IdleTimeout.cc
  JsonWriter.cc
  MimeTypes.cc
  MonotonicClock.cc
  MonotonicTime.cc
  Random.cc
  RegExp.cc
  RuntimeError.cc
  StackTrace.cc
  Status.cc
  StringUtil.cc
  Tokenizer.cc
  UnixTime.cc
  Uri.cc
  UTF8.cc
  WallClock.cc
  inspect.cc

  cli/CLI.cc
  cli/Flags.cc

  executor/SafeCall.cc
  executor/Executor.cc
  executor/LocalExecutor.cc
  executor/LocalScheduler.cc
  executor/PosixScheduler.cc
  executor/Scheduler.cc
  executor/ThreadedExecutor.cc
  executor/ThreadPool.cc

  hash/FNV.cc

  io/GzipFilter.cc
  io/File.cc
  io/FileDescriptor.cc
  io/FileRef.cc
  io/FileRepository.cc
  io/Filter.cc
  io/FileUtil.cc
  io/LocalFile.cc
  io/LocalFileRepository.cc
  io/MemoryFile.cc
  io/MemoryFileRepository.cc
  io/MemoryMap.cc
  io/PageManager.cc
  io/InputStream.cc
  io/FileInputStream.cc
  io/BufferInputStream.cc
  io/StringInputStream.cc
  io/OutputStream.cc
  io/FileOutputStream.cc
  io/BufferOutputStream.cc
  io/StringOutputStream.cc

  logging/ConsoleLogTarget.cc
  logging/LogLevel.cc
  logging/LogTarget.cc
  logging/Logger.cc

  net/ByteArrayEndPoint.cc
  net/Cidr.cc
  net/Connection.cc
  net/ConnectionFactory.cc
  net/Connector.cc
  net/DatagramConnector.cc
  net/DatagramEndPoint.cc
  net/DnsClient.cc
  net/EndPoint.cc
  net/EndPointWriter.cc
  net/InetConnector.cc
  net/InetEndPoint.cc
  net/LocalConnector.cc
  net/LocalDatagramConnector.cc
  net/LocalDatagramEndPoint.cc
  net/Server.cc
  net/SslConnector.cc
  net/SslContext.cc
  net/SslEndPoint.cc
  net/UdpClient.cc
  net/UdpConnector.cc
  net/UdpEndPoint.cc

  thread/SignalHandler.cc
  thread/Wakeup.cc

  util/BinaryMessageReader.cc
  util/BinaryMessageWriter.cc
  util/SimpleRateLimit.cc

  # HTTP common
  http/BadMessage.cc
  http/Cookies.cc
  http/HeaderField.cc
  http/HeaderFieldList.cc
  http/HttpBufferedInput.cc
  http/HttpChannel.cc
  http/HttpConnectionFactory.cc
  http/HttpDateGenerator.cc
  http/HttpFileHandler.cc
  http/HttpInput.cc
  http/HttpListener.cc
  http/HttpMethod.cc
  http/HttpOutput.cc
  http/HttpOutputCompressor.cc
  http/HttpRequest.cc
  http/HttpRequestInfo.cc
  http/HttpResponse.cc
  http/HttpService.cc
  http/HttpStatus.cc
  http/HttpTransport.cc
  http/HttpVersion.cc

  # HTTP transport: mock
  http/mock/Transport.cc
  http/mock/Input.cc

  # HTTP transport: fastcgi
  http/fastcgi/bits.cc
  http/fastcgi/Connection.cc
  http/fastcgi/ConnectionFactory.cc
  http/fastcgi/Generator.cc
  http/fastcgi/RequestParser.cc
  http/fastcgi/ResponseParser.cc

  # HTTP transport: http/1.1
  http/http1/Channel.cc
  http/http1/ConnectionFactory.cc
  http/http1/Connection.cc
  http/http1/Generator.cc
  http/http1/Parser.cc

  # HTTP client
  http/client/HttpClient.cc
  http/client/HttpTransport.cc
  http/client/Http1Connection.cc
  http/client/HttpCluster.cc
  http/client/HttpClusterMember.cc
  http/client/HttpClusterRequest.cc
  http/client/HttpClusterScheduler.cc
  http/client/HttpHealthCheck.cc
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sysconfig.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.h)

set(XZERO_BASE_INCLUDE_DIRS
    ${PCRE_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(XZERO_BASE_INCLUDE_DIRS ${XZERO_BASE_INCLUDE_DIRS} PARENT_SCOPE)

include_directories(${XZERO_BASE_INCLUDE_DIRS})

set(XZERO_BASE_LIBRARIES pthread dl
    ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${PCRE_LIBRARIES} ${RT_LIBRARIES})

# libxzero-base.a
add_library(xzero-base STATIC ${xzero_base_SRC})
target_link_libraries(xzero-base ${XZERO_BASE_LIBRARIES})
#set_target_properties(xzero-base PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_XZERO_BASE=1")
#install(TARGETS xzero-base DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

# headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        FILES_MATCHING PATTERN "*.h"
                       PATTERN "CMakeFiles" EXCLUDE)

# test-base
file(GLOB_RECURSE xzero_base_test_SRC "*-test.cc")
add_executable(test-base ${xzero_base_test_SRC})
target_link_libraries(test-base xzero-base gtest gtest_main)

# pkg-config target
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xzero-base.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/xzero-base.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xzero-base.pc 
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
