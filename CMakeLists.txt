cmake_minimum_required(VERSION 3.6)
project(x0 VERSION "0.11.0" LANGUAGES CXX C)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(ENABLE_FLOW_DIRECT_THREADED_VM "Flow VM using direct threaded mode [default: OFF]" OFF)
option(ENABLE_TESTS "Enable building tests [default: ON]" ON)
option(ENABLE_TIDY "Enable clang-tidy [default: OFF]" OFF)

set(X0D_PIDFILE "${CMAKE_INSTALL_PREFIX}/run/x0d/x0d.pid" CACHE STRING "x0d PID file")
set(X0D_LOGDIR "${CMAKE_INSTALL_PREFIX}/var/log/x0d" CACHE STRING "x0d log directory")
set(X0D_CONFIGFILE "${CMAKE_INSTALL_PREFIX}/etc/x0d/x0d.conf" CACHE STRING "x0d configuration file")
set(XZERO_CLUSTERDIR "${CMAKE_INSTALL_PREFIX}/var/x0d" CACHE STRING "Xzero HTTP cluster directory")
set(PACKAGE_URL "https://xzero.io/")

set(CLANG_TIDY_CHECKS "-checks=bugprone-*,cppcoreguidelines-*,performance-*,clang-analyzer-*,modernize-*,-cppcoreguidelines-pro-type-vararg,-cppcoreguidelines-pro-bounds-array-to-pointer-decay,-cppcoreguidelines-pro-bounds-constant-array-index,-cppcoreguidelines-pro-type-union-access" CACHE STRING "clang-tidy checks to enable/disable")

find_package(Git)
include(ExternalProject)

if(GIT_FOUND)
    ExternalProject_Add(
        fmt
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG master
        CMAKE_ARGS -DMASTER_PROJECT=OFF
                   -DFMT_DOC=OFF
                   -DFMT_INSTALL=ON
                   -DFMT_TEST=OFF
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        LOG_DOWNLOAD 1
        UPDATE_DISCONNECTED 1
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config ${configuration}
        INSTALL_COMMAND ""
    )

	if(UNIX)
		message(STATUS "Hello UNIX")
		set(FMT_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/src/fmt-build/libfmt.a")
	else()
		message(STATUS "Hello Windows")
		set(FMT_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/src/fmt-build/fmt.lib")
	endif()
else()
    # assume catch is installed in a system directory
    #add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt")
endif()

if(ENABLE_TIDY)
  find_program(CLANG_TIDY_EXE
    NAMES clang-tidy-7 clang-tidy-6.0 clang-tidy
    DOC "Path to clang-tidy executable")
  if(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found.")
  else()
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "${CLANG_TIDY_CHECKS}")
  endif()
endif()

# {{{ build-time requirements and platform feature tests
include(XzeroCommon)
include(CompileMimeTypes)
include(FindDoxygen)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CMakeDetermineCCompiler)

if(UNIX)
  include(FindPkgConfig)
  include(FindOpenSSL)

  if (OPENSSL_FOUND)
    message(STATUS "OpenSSL found ;-)")
    set(XZERO_ENABLE_OPENSSL TRUE)
  else()
    message(FATAL_ERROR "OpenSSL not found ;-(")
  endif()

  CHECK_INCLUDE_FILES(glob.h HAVE_GLOB_H)
  CHECK_INCLUDE_FILES(sys/sendfile.h HAVE_SYS_SENDFILE_H)
  CHECK_FUNCTION_EXISTS(sendfile HAVE_SENDFILE)
  CHECK_FUNCTION_EXISTS(posix_fadvise HAVE_POSIX_FADVISE)
  CHECK_FUNCTION_EXISTS(readahead HAVE_READAHEAD)
  CHECK_FUNCTION_EXISTS(pread HAVE_PREAD)
  CHECK_FUNCTION_EXISTS(gmtime_r HAVE_GMTIME_R)
  CHECK_FUNCTION_EXISTS(getrlimit HAVE_GETRLIMIT)

  CHECK_INCLUDE_FILES(fcntl.h HAVE_FCNTL_H)
  CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
  CHECK_INCLUDE_FILES(sys/inotify.h HAVE_SYS_INOTIFY_H)
  CHECK_INCLUDE_FILES(sys/wait.h HAVE_SYS_WAIT_H)
  CHECK_INCLUDE_FILES(sys/time.h HAVE_SYS_TIME_H)
  CHECK_INCLUDE_FILES(sys/resource.h HAVE_SYS_RESOURCE_H)
  CHECK_INCLUDE_FILES(sys/mman.h HAVE_SYS_MMAN_H)
  CHECK_INCLUDE_FILES(sys/limits.h HAVE_SYS_LIMITS_H)
  CHECK_INCLUDE_FILES(pwd.h HAVE_PWD_H)
  CHECK_INCLUDE_FILES(syslog.h HAVE_SYSLOG_H)
  CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
  CHECK_INCLUDE_FILES(execinfo.h HAVE_EXECINFO_H)
  CHECK_INCLUDE_FILES(uuid/uuid.h HAVE_UUID_UUID_H)
  CHECK_INCLUDE_FILES(security/pam_appl.h HAVE_SECURITY_PAM_APPL_H)

  CHECK_FUNCTION_EXISTS(strptime HAVE_STRPTIME)
  CHECK_FUNCTION_EXISTS(sysconf HAVE_SYSCONF)
  CHECK_FUNCTION_EXISTS(pathconf HAVE_PATHCONF)
  CHECK_FUNCTION_EXISTS(accept4 HAVE_ACCEPT4)
  CHECK_FUNCTION_EXISTS(pipe2 HAVE_PIPE2)
  CHECK_FUNCTION_EXISTS(dup2 HAVE_DUP2)
  CHECK_FUNCTION_EXISTS(dladdr HAVE_DLADDR)
  CHECK_FUNCTION_EXISTS(fork HAVE_FORK)
  CHECK_FUNCTION_EXISTS(backtrace HAVE_BACKTRACE)
  CHECK_FUNCTION_EXISTS(backtrace_symbols HAVE_BACKTRACE_SYMBOLS)
  CHECK_FUNCTION_EXISTS(clock_gettime HAVE_CLOCK_GETTIME)
  CHECK_FUNCTION_EXISTS(sigprocmask HAVE_SIGPROCMASK)

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
endif(UNIX)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/src
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
                    ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/src/fmt/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/xzero/sysconfig.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/src/xzero/sysconfig.h)
# }}}
# {{{ libXzero
set(libXzero_SRC src/xzero/AnsiColor.cc
                 src/xzero/Application.cc
                 src/xzero/Buffer.cc
                 src/xzero/BufferUtil.cc
                 src/xzero/CivilTime.cc
                 src/xzero/Counter.cc
                 src/xzero/DeadlineTimer.cc
                 src/xzero/Duration.cc
                 src/xzero/ExceptionHandler.cc
                 src/xzero/Flags.cc
                 src/xzero/HugeBuffer.cc
                 src/xzero/IEEE754.cc
                 src/xzero/ISO8601.cc
                 src/xzero/JsonWriter.cc
                 src/xzero/MimeTypes.cc
                 src/xzero/MonotonicClock.cc
                 src/xzero/MonotonicTime.cc
                 src/xzero/PosixSignals.cc
                 src/xzero/Random.cc
                 src/xzero/RegExp.cc
                 src/xzero/RuntimeError.cc
                 src/xzero/StackTrace.cc
                 src/xzero/StringUtil.cc
                 src/xzero/TokenShaper.cc
                 src/xzero/Tokenizer.cc
                 src/xzero/UTF8.cc
                 src/xzero/UnixSignals.cc
                 src/xzero/UnixTime.cc
                 src/xzero/Uri.cc
                 src/xzero/WallClock.cc
                 src/xzero/base64.cc
                 src/xzero/base64url.cc
                 src/xzero/executor/Executor.cc
                 src/xzero/executor/LocalExecutor.cc
                 src/xzero/executor/SafeCall.cc
                 src/xzero/executor/ThreadPool.cc
                 src/xzero/executor/ThreadedExecutor.cc
                 src/xzero/hash/FNV.cc
                 src/xzero/http/BadMessage.cc
                 src/xzero/http/Cookies.cc
                 src/xzero/http/HeaderField.cc
                 src/xzero/http/HeaderFieldList.cc
                 src/xzero/http/HttpChannel.cc
                 src/xzero/http/HttpConnectionFactory.cc
                 src/xzero/http/HttpDateGenerator.cc
                 src/xzero/http/HttpFileHandler.cc
                 src/xzero/http/HttpListener.cc
                 src/xzero/http/HttpMethod.cc
                 src/xzero/http/HttpOutputCompressor.cc
                 src/xzero/http/HttpRangeDef.cc
                 src/xzero/http/HttpRequest.cc
                 src/xzero/http/HttpRequestInfo.cc
                 src/xzero/http/HttpResponse.cc
                 src/xzero/http/HttpService.cc
                 src/xzero/http/HttpStatus.cc
                 src/xzero/http/HttpTransport.cc
                 src/xzero/http/HttpVersion.cc
                 src/xzero/http/MediaRange.cc
                 src/xzero/http/client/Http1Connection.cc
                 src/xzero/http/client/HttpClient.cc
                 src/xzero/http/client/HttpTransport.cc
                 src/xzero/http/cluster/ApiHandler.cc
                 src/xzero/http/cluster/Backend.cc
                 src/xzero/http/cluster/Cluster.cc
                 src/xzero/http/cluster/Context.cc
                 src/xzero/http/cluster/HealthMonitor.cc
                 src/xzero/http/cluster/Scheduler.cc
                 src/xzero/http/fastcgi/Connection.cc
                 src/xzero/http/fastcgi/ConnectionFactory.cc
                 src/xzero/http/fastcgi/Generator.cc
                 src/xzero/http/fastcgi/RequestParser.cc
                 src/xzero/http/fastcgi/ResponseParser.cc
                 src/xzero/http/fastcgi/bits.cc
                 src/xzero/http/hpack/DynamicTable.cc
                 src/xzero/http/hpack/Generator.cc
                 src/xzero/http/hpack/Huffman.cc
                 src/xzero/http/hpack/Parser.cc
                 src/xzero/http/hpack/StaticTable.cc
                 src/xzero/http/http1/Channel.cc
                 src/xzero/http/http1/Connection.cc
                 src/xzero/http/http1/ConnectionFactory.cc
                 src/xzero/http/http1/Generator.cc
                 src/xzero/http/http1/Parser.cc
                 src/xzero/http/http2/Connection.cc
                 src/xzero/http/http2/ConnectionFactory.cc
                 src/xzero/http/http2/ErrorCode.cc
                 src/xzero/http/http2/FrameGenerator.cc
                 src/xzero/http/http2/FrameParser.cc
                 src/xzero/http/http2/FrameType.cc
                 src/xzero/http/http2/SettingParameter.cc
                 src/xzero/http/http2/Settings.cc
                 src/xzero/http/http2/Stream.cc
                 src/xzero/http/mock/Transport.cc
                 src/xzero/inspect.cc
                 src/xzero/io/DataChain.cc
                 src/xzero/io/File.cc
                 src/xzero/io/FileDescriptor.cc
                 src/xzero/io/FileRepository.cc
                 src/xzero/io/FileUtil.cc
                 src/xzero/io/FileView.cc
                 src/xzero/io/Filter.cc
                 src/xzero/io/LocalFile.cc
                 src/xzero/io/LocalFileRepository.cc
                 src/xzero/io/MemoryFile.cc
                 src/xzero/io/MemoryFileRepository.cc
                 src/xzero/io/SystemPipe.cc
                 src/xzero/logging.cc
                 src/xzero/net/Cidr.cc
                 src/xzero/net/DnsClient.cc
                 src/xzero/net/EndPointWriter.cc
                 src/xzero/net/InetAddress.cc
                 src/xzero/net/TcpConnection.cc
                 src/xzero/net/TcpConnector.cc
                 src/xzero/net/TcpEndPoint.cc
                 src/xzero/net/TcpUtil.cc
                 src/xzero/net/UdpClient.cc
                 src/xzero/net/UdpConnector.cc
                 src/xzero/net/UdpEndPoint.cc
                 src/xzero/raft/Discovery.cc
                 src/xzero/raft/Error.cc
                 src/xzero/raft/Generator.cc
                 src/xzero/raft/InetTransport.cc
                 src/xzero/raft/LocalTransport.cc
                 src/xzero/raft/Parser.cc
                 src/xzero/raft/Server.cc
                 src/xzero/raft/ServerUtil.cc
                 src/xzero/raft/Storage.cc
                 src/xzero/raft/Transport.cc
                 src/xzero/raft/rpc.cc
                 src/xzero/testing.cc
                 src/xzero/text/IniFile.cc
                 src/xzero/thread/Queue.cc
                 src/xzero/thread/SignalHandler.cc
                 src/xzero/thread/Wakeup.cc
                 src/xzero/util/BinaryMessageReader.cc
                 src/xzero/util/BinaryMessageWriter.cc
                 src/xzero/util/BinaryReader.cc
                 src/xzero/util/BinaryWriter.cc
                 src/xzero/util/SimpleRateLimit.cc
                 )

if(OPENSSL_FOUND)
  set(libXzero_SRC ${libXzero_SRC}
                   src/xzero/net/SslConnector.cc
                   src/xzero/net/SslContext.cc
                   src/xzero/net/SslEndPoint.cc)
endif()
if(HAVE_ZLIB_H)
  set(libXzero_SRC ${libXzero_SRC} src/xzero/io/GzipFilter.cc)
endif()
if(UNIX)
  set(libXzero_SRC ${libXzero_SRC} src/xzero/executor/PosixScheduler.cc)
endif()
if(UNIX AND NOT APPLE) 
  set(libXzero_SRC ${libXzero_SRC}
                   src/xzero/LinuxSignals.cc
                   src/xzero/executor/LinuxScheduler.cc)
endif()
if(APPLE)
  set(libXzero_SRC ${libXzero_SRC} src/xzero/KQueueSignals.cc)
endif()
# if(MSVC)
#   set(libXzero_SRC ${libXzero_SRC} src/xzero/WinSockScheduler.cc)
# endif()

add_library(Xzero STATIC ${libXzero_SRC})
add_dependencies(Xzero fmt)
target_compile_definitions(Xzero PRIVATE XZERO_CLUSTERDIR="${XZERO_CLUSTERDIR}")
set_target_properties(Xzero PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(Xzero PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
target_link_libraries(Xzero ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${FMT_LIBRARIES})
if(UNIX)
  target_link_libraries(Xzero pthread dl stdc++fs)
endif()
if(NOT APPLE)
  target_link_libraries(Xzero rt)
endif()

install(TARGETS Xzero
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/xzero/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/include/xzero"
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/xzero/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/include/xzero"
        FILES_MATCHING PATTERN "*.h")
# }}}
# {{{ libXzeroTesting
set(libXzeroTest_SRC src/xzero/testing_main.cc)
add_library(XzeroTest STATIC ${libXzeroTest_SRC})
target_link_libraries(XzeroTest Xzero)
install(TARGETS XzeroTest DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
# }}}
# {{{ libXzeroFlow
set(XzeroFlow_SRC src/xzero-flow/AST.cc
                  src/xzero-flow/ASTPrinter.cc
                  src/xzero-flow/FlowCallVisitor.cc
                  src/xzero-flow/FlowLexer.cc
                  src/xzero-flow/FlowParser.cc
                  src/xzero-flow/FlowToken.cc
                  src/xzero-flow/LiteralType.cc
                  src/xzero-flow/IRGenerator.cc
                  src/xzero-flow/MatchClass.cc
                  src/xzero-flow/NativeCallback.cc
                  src/xzero-flow/Signature.cc
                  src/xzero-flow/SourceLocation.cc
                  src/xzero-flow/TargetCodeGenerator.cc
                  src/xzero-flow/ir/BasicBlock.cc
                  src/xzero-flow/ir/Constant.cc
                  src/xzero-flow/ir/ConstantArray.cc
                  src/xzero-flow/ir/ConstantValue.cc
                  src/xzero-flow/ir/IRBuilder.cc
                  src/xzero-flow/ir/IRHandler.cc
                  src/xzero-flow/ir/IRProgram.cc
                  src/xzero-flow/ir/Instr.cc
                  src/xzero-flow/ir/InstructionVisitor.cc
                  src/xzero-flow/ir/Instructions.cc
                  src/xzero-flow/ir/PassManager.cc
                  src/xzero-flow/ir/Value.cc
                  src/xzero-flow/transform/EmptyBlockElimination.cc
                  src/xzero-flow/transform/InstructionElimination.cc
                  src/xzero-flow/transform/MergeBlockPass.cc
                  src/xzero-flow/transform/UnusedBlockPass.cc
                  src/xzero-flow/vm/ConstantPool.cc
                  src/xzero-flow/vm/Handler.cc
                  src/xzero-flow/vm/Instruction.cc
                  src/xzero-flow/vm/Match.cc
                  src/xzero-flow/vm/Program.cc
                  src/xzero-flow/vm/Runner.cc
                  src/xzero-flow/vm/Runtime.cc
                  )

add_library(XzeroFlow STATIC ${XzeroFlow_SRC})
set_target_properties(XzeroFlow PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(XzeroFlow PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
target_link_libraries(XzeroFlow Xzero)
install(TARGETS XzeroFlow DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/xzero-flow/"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/include/xzero-flow"
    FILES_MATCHING PATTERN "*.h")
# }}}
# {{{ x0d
MIMETYPES_GENERATE_CPP(MT_SRC ${PROJECT_SOURCE_DIR}/3rdparty/mime.types)

set(x0d_SRC ${MT_SRC}
            src/x0d/Context.cc
            src/x0d/Daemon.cc
            src/x0d/Module.cc
            src/x0d/main.cc
            src/x0d/modules/access.cc
            src/x0d/modules/accesslog.cc
            src/x0d/modules/auth.cc
            src/x0d/modules/compress.cc
            src/x0d/modules/core.cc
            src/x0d/modules/dirlisting.cc
            src/x0d/modules/empty_gif.cc
            src/x0d/modules/userdir.cc
            src/x0d/modules/webdav.cc
)

add_executable(x0d ${x0d_SRC})
target_compile_definitions(x0d PRIVATE
                           PACKAGE_URL="${PACKAGE_URL}"
                           X0D_PIDFILE="${X0D_PIDFILE}"
                           X0D_LOGDIR="${X0D_LOGDIR}"
                           X0D_CONFIGFILE="${X0D_CONFIGFILE}")
set_target_properties(x0d PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(x0d PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
target_link_libraries(x0d Xzero XzeroFlow pam)
install(TARGETS x0d
        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/x0d/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/include/x0d"
        FILES_MATCHING PATTERN "*.h")
# }}}
# {{{ test
if(ENABLE_TESTS)
set(xzero_test_SRC
		src/xzero-flow/FlowLexer-test.cc
		src/xzero-flow/FlowParser-test.cc
		src/xzero/Base64-test.cc
		src/xzero/Buffer-test.cc
		src/xzero/Callback-test.cc
		src/xzero/Channel-test.cc
		src/xzero/DeadlineTimer-test.cc
		src/xzero/Flags-test.cc
		src/xzero/HugeBuffer-test.cc
		src/xzero/MimeTypes-test.cc
		src/xzero/PrefixTree-test.cc
		src/xzero/Result-test.cc
		src/xzero/StringUtil-test.cc
		src/xzero/SuffixTree-test.cc
		src/xzero/TokenShaper-test.cc
		src/xzero/Tokenizer-test.cc
		src/xzero/Uri-test.cc
		src/xzero/executor/PosixScheduler-test.cc
		src/xzero/executor/ThreadPool-test.cc
		src/xzero/hash/FNV-test.cc
		src/xzero/http/HeaderField-test.cc
		src/xzero/http/HeaderFieldList-test.cc
		src/xzero/http/HttpChannel-test.cc
		src/xzero/http/HttpFileHandler-test.cc
		src/xzero/http/HttpRangeDef-test.cc
		src/xzero/http/HttpRequestInfo-test.cc
		src/xzero/http/MediaRange-test.cc
		src/xzero/http/client/HttpClient-test.cc
		src/xzero/http/fastcgi/Generator-test.cc
		src/xzero/http/fastcgi/RequestParser-test.cc
		src/xzero/http/fastcgi/ResponseParser-test.cc
		src/xzero/http/hpack/DynamicTable-test.cc
		src/xzero/http/hpack/Generator-test.cc
		src/xzero/http/hpack/Parser-test.cc
		src/xzero/http/hpack/StaticTable-test.cc
		src/xzero/http/http1/Connection-test.cc
		src/xzero/http/http1/Generator-test.cc
		src/xzero/http/http1/Parser-test.cc
		src/xzero/http/http2/Connection-test.cc
		src/xzero/http/http2/Flow-test.cc
		src/xzero/http/http2/FrameGenerator-test.cc
		src/xzero/http/http2/FrameParser-test.cc
		src/xzero/http/http2/Stream-test.cc
		src/xzero/io/DataChain-test.cc
		src/xzero/io/MemoryFile-test.cc
		src/xzero/net/Cidr-test.cc
		src/xzero/net/TcpConnector-test.cc
		src/xzero/raft/Generator-test.cc
		src/xzero/raft/InetTransport-test.cc
		src/xzero/raft/Parser-test.cc
		src/xzero/raft/Server-test.cc
		src/xzero/raft/ServerUtil-test.cc
		src/xzero/thread/Future-test.cc
		src/xzero/thread/Queue-test.cc
		src/xzero/util/BinaryReader-test.cc
		src/xzero/util/BinaryWriter-test.cc
    )

if(UNIX AND NOT APPLE) # i.e. LINUX
  set(xzero_test_PLATFORM_SRC src/xzero/executor/LinuxScheduler-test.cc)
else()
  set(xzero_test_PLATFORM_SRC)
endif()

add_executable(xzero_test ${xzero_test_SRC} ${xzero_test_PLATFORM_SRC})
set_target_properties(xzero_test PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(xzero_test PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
target_link_libraries(xzero_test Xzero XzeroFlow XzeroTest)

add_executable(flowtest src/xzero-flow/flowtest.cc)
set_target_properties(flowtest PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(flowtest PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
target_link_libraries(flowtest Xzero XzeroFlow stdc++fs)

endif(ENABLE_TESTS)
# }}}
# {{{ examples
add_executable(hello_http examples/hello_http.cc)
target_link_libraries(hello_http Xzero)
set_target_properties(hello_http PROPERTIES	CXX_STANDARD 17	CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(hello_http PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
# }}}
