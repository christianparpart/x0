# This file is part of the "x0" project, http://github.com/christianparpart/x0>
#   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
#
# Licensed under the MIT License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of
# the License at: http://opensource.org/licenses/MIT

EXTRA_DIST = autogen.sh
bin_PROGRAMS =
noinst_LIBRARIES =
check_PROGRAMS =
TESTS =

####### compile opts ##########################################################

AM_CXXFLAGS = \
	-DXOPEN_SOURCE=600 -D_FILE_OFFSET_BITS=64 \
	-D_GNU_SOURCE \
	-D_LARGEFILE_SOURCE -D_LARGE_FILES \
	-Wall -Wextra -std=c++11 \
	-Wno-unused-variable \
	-Wno-unused-parameter \
	-DXZERO_CLUSTERDIR="\"$(XZERO_CLUSTERDIR)\"" \
	$(PLATFORM_CXXFLAGS) \
	$(LIBPCRE_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src

AM_LDFLAGS = \
	$(PLATFORM_LDFLAGS) \
	$(OPENSSL_LDADD) \
	$(LIBPCRE_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BZLIB_LIBS) \
	-lpthread -ldl

####### libXzeroTesting_main.a ################################################

noinst_LIBRARIES += libXzeroTesting_main.a
libXzeroTesting_main_a_SOURCES = src/xzero/testing_main.cc
libXzeroTesting_main_a_LIBADD = libXzero.a

####### libXzero.a ############################################################

noinst_LIBRARIES += libXzero.a

libXzero_a_SOURCES = \
	src/xzero/AnsiColor.cc \
	src/xzero/AnsiColor.h \
	src/xzero/Api.h \
	src/xzero/Application.cc \
	src/xzero/Application.h \
	src/xzero/Buffer.cc \
	src/xzero/Buffer.h \
	src/xzero/BufferUtil.cc \
	src/xzero/BufferUtil.h \
	src/xzero/Callback.h \
	src/xzero/CivilTime.cc \
	src/xzero/CivilTime.h \
	src/xzero/CivilTime_impl.h \
	src/xzero/CompletionHandler.h \
	src/xzero/Counter.cc \
	src/xzero/Counter.h \
	src/xzero/CustomDataMgr.h \
	src/xzero/Duration.cc \
	src/xzero/Duration.h \
	src/xzero/Duration_impl.h \
	src/xzero/ExceptionHandler.cc \
	src/xzero/ExceptionHandler.h \
	src/xzero/HugeBuffer.cc \
	src/xzero/HugeBuffer.h \
	src/xzero/IEEE754.cc \
	src/xzero/IEEE754.h \
	src/xzero/ISO8601.cc \
	src/xzero/ISO8601.h \
	src/xzero/IdleTimeout.cc \
	src/xzero/IdleTimeout.h \
	src/xzero/JsonWriter.cc \
	src/xzero/JsonWriter.h \
	src/xzero/LinkedCounter.h \
	src/xzero/MimeTypes.cc \
	src/xzero/MimeTypes.h \
	src/xzero/MonotonicClock.cc \
	src/xzero/MonotonicClock.h \
	src/xzero/MonotonicTime.cc \
	src/xzero/MonotonicTime.h \
	src/xzero/MonotonicTime_impl.h \
	src/xzero/Option-inl.h \
	src/xzero/Option.h \
	src/xzero/PrefixTree.h \
	src/xzero/Random.cc \
	src/xzero/Random.h \
	src/xzero/RefCounted.h \
	src/xzero/RefPtr.h \
	src/xzero/RegExp.cc \
	src/xzero/RegExp.h \
	src/xzero/RuntimeError.cc \
	src/xzero/RuntimeError.h \
	src/xzero/StackTrace.cc \
	src/xzero/StackTrace.h \
	src/xzero/Status.cc \
	src/xzero/Status.h \
	src/xzero/StringUtil-impl.h \
	src/xzero/StringUtil.cc \
	src/xzero/StringUtil.h \
	src/xzero/SuffixTree.h \
	src/xzero/TokenShaper-inl.h \
	src/xzero/TokenShaper.cc \
	src/xzero/TokenShaper.h \
	src/xzero/Tokenizer.cc \
	src/xzero/Tokenizer.h \
	src/xzero/UTF8.cc \
	src/xzero/UTF8.h \
	src/xzero/UnixSignalInfo.h \
	src/xzero/UnixSignals.cc \
	src/xzero/UnixSignals.h \
	src/xzero/UnixTime.cc \
	src/xzero/UnixTime.h \
	src/xzero/UnixTime_impl.h \
	src/xzero/Uri.cc \
	src/xzero/Uri.h \
	src/xzero/Utility.h \
	src/xzero/WallClock.cc \
	src/xzero/WallClock.h \
	src/xzero/base64-inl.h \
	src/xzero/base64.cc \
	src/xzero/base64.h \
	src/xzero/base64url-inl.h \
	src/xzero/base64url.cc \
	src/xzero/base64url.h \
	src/xzero/cli/CLI.cc \
	src/xzero/cli/CLI.h \
	src/xzero/cli/FlagType.h \
	src/xzero/cli/Flags.cc \
	src/xzero/cli/Flags.h \
	src/xzero/defines.h \
	src/xzero/executor/EventLoop.h \
	src/xzero/executor/Executor-inl.h \
	src/xzero/executor/Executor.cc \
	src/xzero/executor/Executor.h \
	src/xzero/executor/LocalExecutor.cc \
	src/xzero/executor/LocalExecutor.h \
	src/xzero/executor/NativeScheduler.h \
	src/xzero/executor/PosixScheduler.cc \
	src/xzero/executor/PosixScheduler.h \
	src/xzero/executor/SafeCall.cc \
	src/xzero/executor/SafeCall.h \
	src/xzero/executor/ThreadPool.cc \
	src/xzero/executor/ThreadPool.h \
	src/xzero/executor/ThreadedExecutor.cc \
	src/xzero/executor/ThreadedExecutor.h \
	src/xzero/hash/FNV.cc \
	src/xzero/hash/FNV.h \
	src/xzero/http/Api.h \
	src/xzero/http/BadMessage.cc \
	src/xzero/http/BadMessage.h \
	src/xzero/http/Cookies.cc \
	src/xzero/http/Cookies.h \
	src/xzero/http/HeaderField.cc \
	src/xzero/http/HeaderField.h \
	src/xzero/http/HeaderFieldList.cc \
	src/xzero/http/HeaderFieldList.h \
	src/xzero/http/HttpChannel.cc \
	src/xzero/http/HttpChannel.h \
	src/xzero/http/HttpConnectionFactory.cc \
	src/xzero/http/HttpConnectionFactory.h \
	src/xzero/http/HttpDateGenerator.cc \
	src/xzero/http/HttpDateGenerator.h \
	src/xzero/http/HttpFileHandler.cc \
	src/xzero/http/HttpFileHandler.h \
	src/xzero/http/HttpHandler.h \
	src/xzero/http/HttpInfo.h \
	src/xzero/http/HttpListener.cc \
	src/xzero/http/HttpListener.h \
	src/xzero/http/HttpMethod.cc \
	src/xzero/http/HttpMethod.h \
	src/xzero/http/HttpOutputCompressor.cc \
	src/xzero/http/HttpOutputCompressor.h \
	src/xzero/http/HttpRangeDef-inl.h \
	src/xzero/http/HttpRangeDef.h \
	src/xzero/http/HttpRequest.cc \
	src/xzero/http/HttpRequest.h \
	src/xzero/http/HttpRequestInfo.cc \
	src/xzero/http/HttpRequestInfo.h \
	src/xzero/http/HttpResponse.cc \
	src/xzero/http/HttpResponse.h \
	src/xzero/http/HttpResponseInfo.h \
	src/xzero/http/HttpService.cc \
	src/xzero/http/HttpService.h \
	src/xzero/http/HttpStatus.cc \
	src/xzero/http/HttpStatus.h \
	src/xzero/http/HttpTransport.cc \
	src/xzero/http/HttpTransport.h \
	src/xzero/http/HttpVersion.cc \
	src/xzero/http/HttpVersion.h \
	src/xzero/http/client/ClientAbortAction.h \
	src/xzero/http/client/Http1Connection.cc \
	src/xzero/http/client/Http1Connection.h \
	src/xzero/http/client/HttpCache.h \
	src/xzero/http/client/HttpClient.cc \
	src/xzero/http/client/HttpClient.h \
	src/xzero/http/client/HttpCluster.cc \
	src/xzero/http/client/HttpCluster.h \
	src/xzero/http/client/HttpClusterApi.h \
	src/xzero/http/client/HttpClusterApiHandler.cc \
	src/xzero/http/client/HttpClusterApiHandler.h \
	src/xzero/http/client/HttpClusterMember.cc \
	src/xzero/http/client/HttpClusterMember.h \
	src/xzero/http/client/HttpClusterRequest.cc \
	src/xzero/http/client/HttpClusterRequest.h \
	src/xzero/http/client/HttpClusterScheduler.cc \
	src/xzero/http/client/HttpClusterScheduler.h \
	src/xzero/http/client/HttpClusterSchedulerStatus.h \
	src/xzero/http/client/HttpHealthMonitor.cc \
	src/xzero/http/client/HttpHealthMonitor.h \
	src/xzero/http/client/HttpTransport.cc \
	src/xzero/http/client/HttpTransport.h \
	src/xzero/http/fastcgi/Connection.cc \
	src/xzero/http/fastcgi/Connection.h \
	src/xzero/http/fastcgi/ConnectionFactory.cc \
	src/xzero/http/fastcgi/ConnectionFactory.h \
	src/xzero/http/fastcgi/Generator.cc \
	src/xzero/http/fastcgi/Generator.h \
	src/xzero/http/fastcgi/RequestParser.cc \
	src/xzero/http/fastcgi/RequestParser.h \
	src/xzero/http/fastcgi/ResponseParser.cc \
	src/xzero/http/fastcgi/ResponseParser.h \
	src/xzero/http/fastcgi/bits-inl.h \
	src/xzero/http/fastcgi/bits.cc \
	src/xzero/http/fastcgi/bits.h \
	src/xzero/http/hpack/DynamicTable.cc \
	src/xzero/http/hpack/DynamicTable.h \
	src/xzero/http/hpack/Generator.cc \
	src/xzero/http/hpack/Generator.h \
	src/xzero/http/hpack/Huffman.cc \
	src/xzero/http/hpack/Huffman.h \
	src/xzero/http/hpack/Parser.cc \
	src/xzero/http/hpack/Parser.h \
	src/xzero/http/hpack/StaticTable.cc \
	src/xzero/http/hpack/StaticTable.h \
	src/xzero/http/hpack/TableEntry.h \
	src/xzero/http/http1/Channel.cc \
	src/xzero/http/http1/Channel.h \
	src/xzero/http/http1/Client.h \
	src/xzero/http/http1/Connection.cc \
	src/xzero/http/http1/Connection.h \
	src/xzero/http/http1/ConnectionFactory.cc \
	src/xzero/http/http1/ConnectionFactory.h \
	src/xzero/http/http1/Generator.cc \
	src/xzero/http/http1/Generator.h \
	src/xzero/http/http1/Parser.cc \
	src/xzero/http/http1/Parser.h \
	src/xzero/http/http2/Connection.cc \
	src/xzero/http/http2/Connection.h \
	src/xzero/http/http2/ConnectionFactory.cc \
	src/xzero/http/http2/ConnectionFactory.h \
	src/xzero/http/http2/DependencyTree.h \
	src/xzero/http/http2/ErrorCode.cc \
	src/xzero/http/http2/ErrorCode.h \
	src/xzero/http/http2/Flow-inl.h \
	src/xzero/http/http2/Flow.h \
	src/xzero/http/http2/FrameListener.h \
	src/xzero/http/http2/FrameType.cc \
	src/xzero/http/http2/FrameType.h \
	src/xzero/http/http2/Generator.cc \
	src/xzero/http/http2/Generator.h \
	src/xzero/http/http2/Parser.cc \
	src/xzero/http/http2/Parser.h \
	src/xzero/http/http2/SettingParameter.cc \
	src/xzero/http/http2/SettingParameter.h \
	src/xzero/http/http2/Settings.cc \
	src/xzero/http/http2/Settings.h \
	src/xzero/http/http2/Stream.cc \
	src/xzero/http/http2/Stream.h \
	src/xzero/http/http2/StreamID.h \
	src/xzero/http/http2/StreamState.cc \
	src/xzero/http/http2/StreamState.h \
	src/xzero/http/mock/Transport.cc \
	src/xzero/http/mock/Transport.h \
	src/xzero/inspect-impl.h \
	src/xzero/inspect.cc \
	src/xzero/inspect.h \
	src/xzero/io/BufferInputStream.cc \
	src/xzero/io/BufferInputStream.h \
	src/xzero/io/BufferOutputStream.cc \
	src/xzero/io/BufferOutputStream.h \
	src/xzero/io/DataChain.cc \
	src/xzero/io/DataChain.h \
	src/xzero/io/DataChainListener.h \
	src/xzero/io/File.cc \
	src/xzero/io/File.h \
	src/xzero/io/FileDescriptor.cc \
	src/xzero/io/FileDescriptor.h \
	src/xzero/io/FileInputStream.cc \
	src/xzero/io/FileInputStream.h \
	src/xzero/io/FileOutputStream.cc \
	src/xzero/io/FileOutputStream.h \
	src/xzero/io/FileRepository.cc \
	src/xzero/io/FileRepository.h \
	src/xzero/io/FileUtil.cc \
	src/xzero/io/FileUtil.h \
	src/xzero/io/FileView.cc \
	src/xzero/io/FileView.h \
	src/xzero/io/Filter.cc \
	src/xzero/io/Filter.h \
	src/xzero/io/GzipFilter.cc \
	src/xzero/io/GzipFilter.h \
	src/xzero/io/InputStream.cc \
	src/xzero/io/InputStream.h \
	src/xzero/io/LocalFile.cc \
	src/xzero/io/LocalFile.h \
	src/xzero/io/LocalFileRepository.cc \
	src/xzero/io/LocalFileRepository.h \
	src/xzero/io/MemoryFile.cc \
	src/xzero/io/MemoryFile.h \
	src/xzero/io/MemoryFileRepository.cc \
	src/xzero/io/MemoryFileRepository.h \
	src/xzero/io/MemoryMap.cc \
	src/xzero/io/MemoryMap.h \
	src/xzero/io/OutputStream.cc \
	src/xzero/io/OutputStream.h \
	src/xzero/io/StringInputStream.cc \
	src/xzero/io/StringInputStream.h \
	src/xzero/io/StringOutputStream.cc \
	src/xzero/io/StringOutputStream.h \
	src/xzero/logging.h \
	src/xzero/logging/ConsoleLogTarget.cc \
	src/xzero/logging/ConsoleLogTarget.h \
	src/xzero/logging/FileLogTarget.cc \
	src/xzero/logging/FileLogTarget.h \
	src/xzero/logging/LogLevel.cc \
	src/xzero/logging/LogLevel.h \
	src/xzero/logging/LogTarget.cc \
	src/xzero/logging/LogTarget.h \
	src/xzero/logging/Logger.cc \
	src/xzero/logging/Logger.h \
	src/xzero/logging/Logger_impl.h \
	src/xzero/net/ByteArrayEndPoint.cc \
	src/xzero/net/ByteArrayEndPoint.h \
	src/xzero/net/Cidr.cc \
	src/xzero/net/Cidr.h \
	src/xzero/net/Connection.cc \
	src/xzero/net/Connection.h \
	src/xzero/net/ConnectionFactory.cc \
	src/xzero/net/ConnectionFactory.h \
	src/xzero/net/Connector.cc \
	src/xzero/net/Connector.h \
	src/xzero/net/DatagramConnector.cc \
	src/xzero/net/DatagramConnector.h \
	src/xzero/net/DatagramEndPoint.cc \
	src/xzero/net/DatagramEndPoint.h \
	src/xzero/net/DnsClient.cc \
	src/xzero/net/DnsClient.h \
	src/xzero/net/EndPoint.cc \
	src/xzero/net/EndPoint.h \
	src/xzero/net/EndPointWriter.cc \
	src/xzero/net/EndPointWriter.h \
	src/xzero/net/IPAddress.h \
	src/xzero/net/InetAddress-inl.h \
	src/xzero/net/InetAddress.cc \
	src/xzero/net/InetAddress.h \
	src/xzero/net/InetConnector.cc \
	src/xzero/net/InetConnector.h \
	src/xzero/net/InetEndPoint.cc \
	src/xzero/net/InetEndPoint.h \
	src/xzero/net/LocalConnector.cc \
	src/xzero/net/LocalConnector.h \
	src/xzero/net/LocalDatagramConnector.cc \
	src/xzero/net/LocalDatagramConnector.h \
	src/xzero/net/LocalDatagramEndPoint.cc \
	src/xzero/net/LocalDatagramEndPoint.h \
	src/xzero/net/Server.cc \
	src/xzero/net/Server.h \
	src/xzero/net/SslConnector.cc \
	src/xzero/net/SslConnector.h \
	src/xzero/net/SslContext.cc \
	src/xzero/net/SslContext.h \
	src/xzero/net/SslEndPoint.cc \
	src/xzero/net/SslEndPoint.h \
	src/xzero/net/UdpClient.cc \
	src/xzero/net/UdpClient.h \
	src/xzero/net/UdpConnector.cc \
	src/xzero/net/UdpConnector.h \
	src/xzero/net/UdpEndPoint.cc \
	src/xzero/net/UdpEndPoint.h \
	src/xzero/reflect.h \
	src/xzero/reflect/indexsequence.h \
	src/xzero/reflect/metaclass.h \
	src/xzero/reflect/methodcall.h \
	src/xzero/reflect/methodcall_impl.h \
	src/xzero/reflect/property.h \
	src/xzero/reflect/property_impl.h \
	src/xzero/stdtypes.h \
	src/xzero/testing.cc \
	src/xzero/testing.h \
	src/xzero/text/IniFile.cc \
	src/xzero/text/IniFile.h \
	src/xzero/thread/Future-impl.h \
	src/xzero/thread/Future.cc \
	src/xzero/thread/Future.h \
	src/xzero/thread/Queue.cc \
	src/xzero/thread/Queue.h \
	src/xzero/thread/SignalHandler.cc \
	src/xzero/thread/SignalHandler.h \
	src/xzero/thread/Wakeup.cc \
	src/xzero/thread/Wakeup.h \
	src/xzero/time_constants.h \
	src/xzero/traits.h \
	src/xzero/util/BinaryMessageReader.cc \
	src/xzero/util/BinaryMessageReader.h \
	src/xzero/util/BinaryMessageWriter.cc \
	src/xzero/util/BinaryMessageWriter.h \
	src/xzero/util/SimpleRateLimit.cc \
	src/xzero/util/SimpleRateLimit.h

# TODO: finalize
# if PLATFORM_LINUX
# libXzero_a_SOURCES += \
# 	src/xzero/executor/LinuxScheduler.cc \
# 	src/xzero/executor/LinuxScheduler.h
# endif

if PLATFORM_DARWIN
# XXX some stuff here, if any
endif

####### libXzeroFlow.a ########################################################

# TODO option(ENABLE_FLOW_DIRECT_THREADED_VM "Flow VM using direct threaded mode [default: off]" OFF)

noinst_LIBRARIES += libXzeroFlow.a

libXzeroFlow_a_SOURCES = \
	src/xzero-flow/AST.cc \
	src/xzero-flow/AST.h \
	src/xzero-flow/ASTPrinter.cc \
	src/xzero-flow/ASTPrinter.h \
	src/xzero-flow/ASTVisitor.h \
	src/xzero-flow/Api.h \
	src/xzero-flow/FlowCallVisitor.cc \
	src/xzero-flow/FlowCallVisitor.h \
	src/xzero-flow/FlowLexer.cc \
	src/xzero-flow/FlowLexer.h \
	src/xzero-flow/FlowLocation.cc \
	src/xzero-flow/FlowLocation.h \
	src/xzero-flow/FlowParser.cc \
	src/xzero-flow/FlowParser.h \
	src/xzero-flow/FlowToken.cc \
	src/xzero-flow/FlowToken.h \
	src/xzero-flow/FlowType.cc \
	src/xzero-flow/FlowType.h \
	src/xzero-flow/IRGenerator.cc \
	src/xzero-flow/IRGenerator.h \
	src/xzero-flow/TargetCodeGenerator.cc \
	src/xzero-flow/TargetCodeGenerator.h \
	src/xzero-flow/ir/BasicBlock.cc \
	src/xzero-flow/ir/BasicBlock.h \
	src/xzero-flow/ir/Constant.cc \
	src/xzero-flow/ir/Constant.h \
	src/xzero-flow/ir/ConstantArray.cc \
	src/xzero-flow/ir/ConstantArray.h \
	src/xzero-flow/ir/ConstantValue.cc \
	src/xzero-flow/ir/ConstantValue.h \
	src/xzero-flow/ir/HandlerPass.h \
	src/xzero-flow/ir/IRBuilder.cc \
	src/xzero-flow/ir/IRBuilder.h \
	src/xzero-flow/ir/IRBuiltinFunction.h \
	src/xzero-flow/ir/IRBuiltinHandler.h \
	src/xzero-flow/ir/IRHandler.cc \
	src/xzero-flow/ir/IRHandler.h \
	src/xzero-flow/ir/IRProgram.cc \
	src/xzero-flow/ir/IRProgram.h \
	src/xzero-flow/ir/Instr.cc \
	src/xzero-flow/ir/Instr.h \
	src/xzero-flow/ir/InstructionVisitor.cc \
	src/xzero-flow/ir/InstructionVisitor.h \
	src/xzero-flow/ir/Instructions.cc \
	src/xzero-flow/ir/Instructions.h \
	src/xzero-flow/ir/PassManager.cc \
	src/xzero-flow/ir/PassManager.h \
	src/xzero-flow/ir/Value.cc \
	src/xzero-flow/ir/Value.h \
	src/xzero-flow/transform/EmptyBlockElimination.cc \
	src/xzero-flow/transform/EmptyBlockElimination.h \
	src/xzero-flow/transform/InstructionElimination.cc \
	src/xzero-flow/transform/InstructionElimination.h \
	src/xzero-flow/transform/UnusedBlockPass.cc \
	src/xzero-flow/transform/UnusedBlockPass.h \
	src/xzero-flow/vm/ConstantPool.cc \
	src/xzero-flow/vm/ConstantPool.h \
	src/xzero-flow/vm/Handler.cc \
	src/xzero-flow/vm/Handler.h \
	src/xzero-flow/vm/Instruction.cc \
	src/xzero-flow/vm/Instruction.h \
	src/xzero-flow/vm/Match.cc \
	src/xzero-flow/vm/Match.h \
	src/xzero-flow/vm/MatchClass.cc \
	src/xzero-flow/vm/MatchClass.h \
	src/xzero-flow/vm/NativeCallback.cc \
	src/xzero-flow/vm/NativeCallback.h \
	src/xzero-flow/vm/Params.h \
	src/xzero-flow/vm/Program.cc \
	src/xzero-flow/vm/Program.h \
	src/xzero-flow/vm/Runner.cc \
	src/xzero-flow/vm/Runner.h \
	src/xzero-flow/vm/Runtime.cc \
	src/xzero-flow/vm/Runtime.h \
	src/xzero-flow/vm/Signature.cc \
	src/xzero-flow/vm/Signature.h

####### TESTS #################################################################

TESTS += xzero_test
check_PROGRAMS += xzero_test

xzero_test_SOURCES = \
	src/xzero-flow/FlowLexer-test.cc \
	src/xzero-flow/FlowParser-test.cc \
	src/xzero/Base64-test.cc \
	src/xzero/Buffer-test.cc \
	src/xzero/Callback-test.cc \
	src/xzero/Option-test.cc \
	src/xzero/RefPtr-test.cc \
	src/xzero/StringUtil-test.cc \
	src/xzero/TokenShaper-test.cc \
	src/xzero/Tokenizer-test.cc \
	src/xzero/Uri-test.cc \
	src/xzero/cli/CLI-test.cc \
	src/xzero/executor/PosixScheduler-test.cc \
	src/xzero/executor/ThreadPool-test.cc \
	src/xzero/hash/FNV-test.cc \
	src/xzero/http/HeaderField-test.cc \
	src/xzero/http/HeaderFieldList-test.cc \
	src/xzero/http/HttpChannel-test.cc \
	src/xzero/http/HttpFileHandler-test.cc \
	src/xzero/http/HttpRangeDef-test.cc \
	src/xzero/http/HttpRequestInfo-test.cc \
	src/xzero/http/client/HttpClient-test.cc \
	src/xzero/http/fastcgi/Generator-test.cc \
	src/xzero/http/fastcgi/RequestParser-test.cc \
	src/xzero/http/fastcgi/ResponseParser-test.cc \
	src/xzero/http/hpack/DynamicTable-test.cc \
	src/xzero/http/hpack/Generator-test.cc \
	src/xzero/http/hpack/Parser-test.cc \
	src/xzero/http/hpack/StaticTable-test.cc \
	src/xzero/http/http1/Connection-test.cc \
	src/xzero/http/http1/Generator-test.cc \
	src/xzero/http/http1/Parser-test.cc \
	src/xzero/http/http2/Connection-test.cc \
	src/xzero/http/http2/Flow-test.cc \
	src/xzero/http/http2/Generator-test.cc \
	src/xzero/http/http2/Parser-test.cc \
	src/xzero/http/http2/Stream-test.cc \
	src/xzero/io/DataChain-test.cc \
	src/xzero/io/MemoryFile-test.cc \
	src/xzero/io/MemoryMap-test.cc \
	src/xzero/net/ByteArrayEndPoint-test.cc \
	src/xzero/net/Cidr-test.cc \
	src/xzero/thread/Future-test.cc \
	src/xzero/thread/Queue-test.cc

xzero_test_LDADD = \
	libXzeroTesting_main.a \
	libXzeroFlow.a \
	libXzero.a

####### x0d ###################################################################

bin_PROGRAMS += x0d

x0d_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	-DX0D_CONFIGFILE="\"$(X0D_CONFIGFILE)\"" \
	-DX0D_LOGDIR="\"$(X0D_LOGDIR)\"" \
	-DX0D_PIDFILE="\"$(X0D_PIDFILE)\""

x0d_LDADD = \
	libXzeroFlow.a \
	libXzero.a

x0d_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PAM_LDFLAGS)

# x0d_LDFLAGS += -static -static-libgcc -static-libstdc++

x0d_SOURCES = \
	$(x0d_types) \
	src/x0d/modules/access.cc \
	src/x0d/modules/accesslog.cc \
	src/x0d/modules/auth.cc \
	src/x0d/modules/compress.cc \
	src/x0d/modules/core.cc \
	src/x0d/modules/dirlisting.cc \
	src/x0d/modules/empty_gif.cc \
	src/x0d/modules/proxy.cc \
	src/x0d/modules/userdir.cc \
	src/x0d/modules/webdav.cc \
	src/x0d/XzeroContext.cc \
	src/x0d/XzeroContext.h \
	src/x0d/XzeroDaemon.cc \
	src/x0d/XzeroDaemon.h \
	src/x0d/XzeroEventHandler.cc \
	src/x0d/XzeroEventHandler.h \
	src/x0d/XzeroModule.cc \
	src/x0d/XzeroModule.h \
	src/x0d/XzeroState.cc \
	src/x0d/XzeroState.h \
	src/x0d/main.cc

nodist_x0d_SOURCES = \
	@builddir@/3rdparty/mime.mt.cc

x0d_types = 3rdparty/mime.types
BUILT_SOURCES = $(x0d_types:.types=.mt.cc)
dist_noinst_DATA = $(x0d_types)

%.mt.cc: %.types
	$(abs_top_srcdir)/mimetypes2cc.sh $^ $@ mimetypes2cc

####### man ###################################################################

man_MANS = x0d.8 x0d.conf.5 flow.7

man: x0d.8 x0d.conf.5 flow.7

x0d.8: docs/x0d.8.pod
	pod2man --name=x0d --section 8 --release --utf8 --stderr \
		--center "x0d - high performance HTTP web server" \
		$< >$@

x0d.conf.5: docs/x0d.conf.5.pod
	pod2man --name=x0d --section 5 --release --utf8 --stderr \
		--center "x0d - high performance HTTP web server configuration" \
		$< >$@

flow.7: docs/flow.7.pod
	pod2man --name=x0d --section 7 --release --utf8 --stderr \
		--center "Flow Language Specification" \
		$< >$@

####### dist ##################################################################

dist_sysconf_DATA = x0d.conf

# vim:ts=8
# EOF
