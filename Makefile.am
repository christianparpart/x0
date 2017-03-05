# This file is part of the "x0" project, http://github.com/christianparpart/x0>
#   (c) 2009-2017 Christian Parpart <christian@parpart.family>
#
# Licensed under the MIT License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of
# the License at: http://opensource.org/licenses/MIT

EXTRA_DIST = autogen.sh
bin_PROGRAMS =
lib_LIBRARIES =
noinst_LIBRARIES =
check_PROGRAMS =
TESTS =

####### compile opts ##########################################################

AM_CXXFLAGS = \
	$(BASE_CXXFLAGS) \
	-DXZERO_CLUSTERDIR="\"$(XZERO_CLUSTERDIR)\"" \
	-I$(top_builddir) \
	-I$(top_srcdir)

AM_LDFLAGS = \
	$(BASE_LIBS) \
	$(PLATFORM_LDFLAGS) \
	$(OPENSSL_LDADD) \
	$(LIBPCRE_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BZLIB_LIBS) \
	-lpthread -ldl

####### libXzeroTesting_main.a ################################################

lib_LIBRARIES += libXzeroTesting_main.a
libXzeroTesting_main_a_SOURCES = xzero/testing_main.cc
libXzeroTesting_main_a_LIBADD = libXzero.a

####### libXzero.a ############################################################

lib_LIBRARIES += libXzero.a

libXzero_a_SOURCES = \
	xzero/AnsiColor.cc \
	xzero/Application.cc \
	xzero/Buffer.cc \
	xzero/BufferUtil.cc \
	xzero/CivilTime.cc \
	xzero/Counter.cc \
	xzero/DeadlineTimer.cc \
	xzero/Duration.cc \
	xzero/ExceptionHandler.cc \
	xzero/HugeBuffer.cc \
	xzero/IEEE754.cc \
	xzero/ISO8601.cc \
	xzero/JsonWriter.cc \
	xzero/MimeTypes.cc \
	xzero/MonotonicClock.cc \
	xzero/MonotonicTime.cc \
	xzero/Random.cc \
	xzero/RegExp.cc \
	xzero/RuntimeError.cc \
	xzero/StackTrace.cc \
	xzero/Status.cc \
	xzero/StringUtil.cc \
	xzero/TokenShaper.cc \
	xzero/Tokenizer.cc \
	xzero/UTF8.cc \
	xzero/UnixSignals.cc \
	xzero/UnixTime.cc \
	xzero/Uri.cc \
	xzero/WallClock.cc \
	xzero/base64.cc \
	xzero/base64url.cc \
	xzero/cli/CLI.cc \
	xzero/cli/Flags.cc \
	xzero/executor/Executor.cc \
	xzero/executor/LocalExecutor.cc \
	xzero/executor/PosixScheduler.cc \
	xzero/executor/SafeCall.cc \
	xzero/executor/ThreadPool.cc \
	xzero/executor/ThreadedExecutor.cc \
	xzero/hash/FNV.cc \
	xzero/http/BadMessage.cc \
	xzero/http/Cookies.cc \
	xzero/http/HeaderField.cc \
	xzero/http/HeaderFieldList.cc \
	xzero/http/HttpChannel.cc \
	xzero/http/HttpConnectionFactory.cc \
	xzero/http/HttpRequest.cc \
	xzero/http/HttpRequestInfo.cc \
	xzero/http/HttpResponse.cc \
	xzero/http/HttpService.cc \
	xzero/http/HttpStatus.cc \
	xzero/http/HttpDateGenerator.cc \
	xzero/http/HttpFileHandler.cc \
	xzero/http/HttpListener.cc \
	xzero/http/HttpMethod.cc \
	xzero/http/HttpOutputCompressor.cc \
	xzero/http/HttpTransport.cc \
	xzero/http/HttpVersion.cc \
	xzero/http/client/Http1Connection.cc \
	xzero/http/client/HttpClient.cc \
	xzero/http/client/HttpCluster.cc \
	xzero/http/client/HttpClusterApiHandler.cc \
	xzero/http/client/HttpClusterMember.cc \
	xzero/http/client/HttpClusterRequest.cc \
	xzero/http/client/HttpClusterScheduler.cc \
	xzero/http/client/HttpTransport.cc \
	xzero/http/client/HttpHealthMonitor.cc \
	xzero/http/fastcgi/Connection.cc \
	xzero/http/fastcgi/ConnectionFactory.cc \
	xzero/http/fastcgi/Generator.cc \
	xzero/http/fastcgi/RequestParser.cc \
	xzero/http/fastcgi/ResponseParser.cc \
	xzero/http/fastcgi/bits.cc \
	xzero/http/hpack/DynamicTable.cc \
	xzero/http/hpack/Generator.cc \
	xzero/http/hpack/Huffman.cc \
	xzero/http/hpack/Parser.cc \
	xzero/http/hpack/StaticTable.cc \
	xzero/http/http1/Channel.cc \
	xzero/http/http1/Connection.cc \
	xzero/http/http1/ConnectionFactory.cc \
	xzero/http/http1/Generator.cc \
	xzero/http/http1/Parser.cc \
	xzero/http/http2/Connection.cc \
	xzero/http/http2/ConnectionFactory.cc \
	xzero/http/http2/ErrorCode.cc \
	xzero/http/http2/FrameType.cc \
	xzero/http/http2/Generator.cc \
	xzero/http/http2/Parser.cc \
	xzero/http/http2/SettingParameter.cc \
	xzero/http/http2/Settings.cc \
	xzero/http/http2/Stream.cc \
	xzero/http/http2/StreamState.cc \
	xzero/http/mock/Transport.cc \
	xzero/inspect.cc \
	xzero/io/BufferInputStream.cc \
	xzero/io/BufferOutputStream.cc \
	xzero/io/DataChain.cc \
	xzero/io/File.cc \
	xzero/io/FileDescriptor.cc \
	xzero/io/FileInputStream.cc \
	xzero/io/FileOutputStream.cc \
	xzero/io/FileRepository.cc \
	xzero/io/FileUtil.cc \
	xzero/io/FileView.cc \
	xzero/io/Filter.cc \
	xzero/io/GzipFilter.cc \
	xzero/io/InputStream.cc \
	xzero/io/LocalFile.cc \
	xzero/io/LocalFileRepository.cc \
	xzero/io/MemoryFile.cc \
	xzero/io/MemoryFileRepository.cc \
	xzero/io/MemoryMap.cc \
	xzero/io/OutputStream.cc \
	xzero/io/StringInputStream.cc \
	xzero/io/StringOutputStream.cc \
	xzero/io/SystemPipe.cc \
	xzero/logging/ConsoleLogTarget.cc \
	xzero/logging/FileLogTarget.cc \
	xzero/logging/LogLevel.cc \
	xzero/logging/LogTarget.cc \
	xzero/logging/Logger.cc \
	xzero/logging/SyslogTarget.cc \
	xzero/net/ByteArrayEndPoint.cc \
	xzero/net/Cidr.cc \
	xzero/net/Connection.cc \
	xzero/net/ConnectionFactory.cc \
	xzero/net/Connector.cc \
	xzero/net/DatagramConnector.cc \
	xzero/net/DatagramEndPoint.cc \
	xzero/net/DnsClient.cc \
	xzero/net/EndPoint.cc \
	xzero/net/EndPointWriter.cc \
	xzero/net/InetAddress.cc \
	xzero/net/InetConnector.cc \
	xzero/net/InetEndPoint.cc \
	xzero/net/LocalConnector.cc \
	xzero/net/LocalDatagramConnector.cc \
	xzero/net/LocalDatagramEndPoint.cc \
	xzero/net/Server.cc \
	xzero/net/SslConnector.cc \
	xzero/net/SslContext.cc \
	xzero/net/SslEndPoint.cc \
	xzero/net/UdpClient.cc \
	xzero/net/UdpConnector.cc \
	xzero/net/UdpEndPoint.cc \
	xzero/raft/Discovery.cc \
	xzero/raft/Error.cc \
	xzero/raft/Generator.cc \
	xzero/raft/InetTransport.cc \
	xzero/raft/LocalTransport.cc \
	xzero/raft/MessageType.cc \
	xzero/raft/Parser.cc \
	xzero/raft/rpc.cc \
	xzero/raft/Server.cc \
	xzero/raft/ServerUtil.cc \
	xzero/raft/Storage.cc \
	xzero/raft/Transport.cc \
	xzero/testing.cc \
	xzero/text/IniFile.cc \
	xzero/thread/Future.cc \
	xzero/thread/Wakeup.cc \
	xzero/thread/Queue.cc \
	xzero/thread/SignalHandler.cc \
	xzero/util/BinaryReader.cc \
	xzero/util/BinaryWriter.cc \
	xzero/util/BinaryMessageReader.cc \
	xzero/util/BinaryMessageWriter.cc \
	xzero/util/SimpleRateLimit.cc

libXzerodir = $(includedir)
nobase_libXzero_HEADERS = \
	xzero/AnsiColor.h \
	xzero/Api.h \
	xzero/Application.h \
	xzero/Buffer.h \
	xzero/BufferUtil.h \
	xzero/Callback.h \
	xzero/CivilTime.h \
	xzero/CivilTime_impl.h \
	xzero/CompletionHandler.h \
	xzero/Counter.h \
	xzero/CustomDataMgr.h \
	xzero/DeadlineTimer.h \
	xzero/Duration.h \
	xzero/Duration_impl.h \
	xzero/ExceptionHandler.h \
	xzero/HugeBuffer.h \
	xzero/IEEE754.h \
	xzero/ISO8601.h \
	xzero/JsonWriter.h \
	xzero/LinkedCounter.h \
	xzero/MimeTypes.h \
	xzero/MonotonicClock.h \
	xzero/MonotonicTime.h \
	xzero/MonotonicTime_impl.h \
	xzero/Option-inl.h \
	xzero/Option.h \
	xzero/PrefixTree.h \
	xzero/Random.h \
	xzero/RefCounted.h \
	xzero/RefPtr.h \
	xzero/RegExp.h \
	xzero/RuntimeError.h \
	xzero/StackTrace.h \
	xzero/Status.h \
	xzero/StringUtil-impl.h \
	xzero/StringUtil.h \
	xzero/SuffixTree.h \
	xzero/TokenShaper-inl.h \
	xzero/TokenShaper.h \
	xzero/Tokenizer.h \
	xzero/UTF8.h \
	xzero/UnixSignalInfo.h \
	xzero/UnixSignals.h \
	xzero/UnixTime.h \
	xzero/UnixTime_impl.h \
	xzero/Uri.h \
	xzero/Utility.h \
	xzero/WallClock.h \
	xzero/base64-inl.h \
	xzero/base64.h \
	xzero/base64url-inl.h \
	xzero/base64url.h \
	xzero/defines.h \
	xzero/inspect-impl.h \
	xzero/inspect.h \
	xzero/logging.h \
	xzero/reflect.h \
	xzero/testing.h \
	xzero/time_constants.h \
	xzero/traits.h \
	xzero/cli/CLI.h \
	xzero/cli/FlagType.h \
	xzero/cli/Flags.h \
	xzero/executor/EventLoop.h \
	xzero/executor/Executor-inl.h \
	xzero/executor/Executor.h \
	xzero/executor/LocalExecutor.h \
	xzero/executor/NativeScheduler.h \
	xzero/executor/PosixScheduler.h \
	xzero/executor/SafeCall.h \
	xzero/executor/ThreadPool.h \
	xzero/executor/ThreadedExecutor.h \
	xzero/hash/FNV.h \
	xzero/http/Api.h \
	xzero/http/BadMessage.h \
	xzero/http/Cookies.h \
	xzero/http/HeaderField.h \
	xzero/http/HeaderFieldList.h \
	xzero/http/HttpChannel.h \
	xzero/http/HttpConnectionFactory.h \
	xzero/http/HttpDateGenerator.h \
	xzero/http/HttpFileHandler.h \
	xzero/http/HttpHandler.h \
	xzero/http/HttpInfo.h \
	xzero/http/HttpListener.h \
	xzero/http/HttpMethod.h \
	xzero/http/HttpOutputCompressor.h \
	xzero/http/HttpRangeDef-inl.h \
	xzero/http/HttpRangeDef.h \
	xzero/http/HttpRequest.h \
	xzero/http/HttpRequestInfo.h \
	xzero/http/HttpResponse.h \
	xzero/http/HttpResponseInfo.h \
	xzero/http/HttpService.h \
	xzero/http/HttpStatus.h \
	xzero/http/HttpTransport.h \
	xzero/http/HttpVersion.h \
	xzero/http/client/ClientAbortAction.h \
	xzero/http/client/Http1Connection.h \
	xzero/http/client/HttpCache.h \
	xzero/http/client/HttpClient.h \
	xzero/http/client/HttpCluster.h \
	xzero/http/client/HttpClusterApi.h \
	xzero/http/client/HttpClusterApiHandler.h \
	xzero/http/client/HttpClusterMember.h \
	xzero/http/client/HttpClusterRequest.h \
	xzero/http/client/HttpClusterScheduler.h \
	xzero/http/client/HttpClusterSchedulerStatus.h \
	xzero/http/client/HttpHealthMonitor.h \
	xzero/http/client/HttpTransport.h \
	xzero/http/fastcgi/Connection.h \
	xzero/http/fastcgi/ConnectionFactory.h \
	xzero/http/fastcgi/Generator.h \
	xzero/http/fastcgi/RequestParser.h \
	xzero/http/fastcgi/ResponseParser.h \
	xzero/http/fastcgi/bits-inl.h \
	xzero/http/fastcgi/bits.h \
	xzero/http/hpack/DynamicTable.h \
	xzero/http/hpack/Generator.h \
	xzero/http/hpack/Huffman.h \
	xzero/http/hpack/Parser.h \
	xzero/http/hpack/StaticTable.h \
	xzero/http/hpack/TableEntry.h \
	xzero/http/http1/Channel.h \
	xzero/http/http1/Client.h \
	xzero/http/http1/Connection.h \
	xzero/http/http1/ConnectionFactory.h \
	xzero/http/http1/Generator.h \
	xzero/http/http1/Parser.h \
	xzero/http/http2/Connection.h \
	xzero/http/http2/ConnectionFactory.h \
	xzero/http/http2/DependencyTree.h \
	xzero/http/http2/ErrorCode.h \
	xzero/http/http2/Flow-inl.h \
	xzero/http/http2/Flow.h \
	xzero/http/http2/FrameListener.h \
	xzero/http/http2/FrameType.h \
	xzero/http/http2/Generator.h \
	xzero/http/http2/Parser.h \
	xzero/http/http2/SettingParameter.h \
	xzero/http/http2/Settings.h \
	xzero/http/http2/Stream.h \
	xzero/http/http2/StreamID.h \
	xzero/http/http2/StreamState.h \
	xzero/http/mock/Transport.h \
	xzero/io/BufferInputStream.h \
	xzero/io/BufferOutputStream.h \
	xzero/io/DataChain.h \
	xzero/io/DataChainListener.h \
	xzero/io/File.h \
	xzero/io/FileDescriptor.h \
	xzero/io/FileInputStream.h \
	xzero/io/FileOutputStream.h \
	xzero/io/FileRepository.h \
	xzero/io/FileUtil.h \
	xzero/io/FileView.h \
	xzero/io/Filter.h \
	xzero/io/GzipFilter.h \
	xzero/io/InputStream.h \
	xzero/io/LocalFile.h \
	xzero/io/LocalFileRepository.h \
	xzero/io/MemoryFile.h \
	xzero/io/MemoryFileRepository.h \
	xzero/io/MemoryMap.h \
	xzero/io/OutputStream.h \
	xzero/io/StringInputStream.h \
	xzero/io/StringOutputStream.h \
	xzero/io/SystemPipe.h \
	xzero/logging/ConsoleLogTarget.h \
	xzero/logging/FileLogTarget.h \
	xzero/logging/LogLevel.h \
	xzero/logging/LogTarget.h \
	xzero/logging/Logger.h \
	xzero/logging/Logger_impl.h \
	xzero/logging/SyslogTarget.h \
	xzero/net/ByteArrayEndPoint.h \
	xzero/net/Cidr.h \
	xzero/net/Connection.h \
	xzero/net/ConnectionFactory.h \
	xzero/net/Connector.h \
	xzero/net/DatagramConnector.h \
	xzero/net/DatagramEndPoint.h \
	xzero/net/DnsClient.h \
	xzero/net/EndPoint.h \
	xzero/net/EndPointWriter.h \
	xzero/net/IPAddress.h \
	xzero/net/InetAddress-inl.h \
	xzero/net/InetAddress.h \
	xzero/net/InetConnector.h \
	xzero/net/InetEndPoint.h \
	xzero/net/LocalConnector.h \
	xzero/net/LocalDatagramConnector.h \
	xzero/net/LocalDatagramEndPoint.h \
	xzero/net/Server.h \
	xzero/net/SslConnector.h \
	xzero/net/SslContext.h \
	xzero/net/SslEndPoint.h \
	xzero/net/UdpClient.h \
	xzero/net/UdpConnector.h \
	xzero/net/UdpEndPoint.h \
	xzero/raft/Discovery.h \
	xzero/raft/Error.h \
	xzero/raft/Generator.h \
	xzero/raft/Listener.h \
	xzero/raft/MessageType.h \
	xzero/raft/Parser.h \
	xzero/raft/rpc.h \
	xzero/raft/Server.h \
	xzero/raft/ServerUtil.h \
	xzero/raft/StateMachine.h \
	xzero/raft/Storage.h \
	xzero/raft/Transport.h \
	xzero/reflect/indexsequence.h \
	xzero/reflect/metaclass.h \
	xzero/reflect/methodcall.h \
	xzero/reflect/methodcall_impl.h \
	xzero/reflect/property.h \
	xzero/reflect/property_impl.h \
	xzero/text/IniFile.h \
	xzero/thread/Future-impl.h \
	xzero/thread/Future.h \
	xzero/thread/Queue.h \
	xzero/thread/SignalHandler.h \
	xzero/thread/Wakeup.h \
	xzero/util/BinaryMessageReader.h \
	xzero/util/BinaryMessageWriter.h \
	xzero/util/SimpleRateLimit.h

# TODO: finalize
if PLATFORM_LINUX
libXzero_a_SOURCES += \
	xzero/executor/LinuxScheduler.cc

nobase_libXzero_HEADERS += \
	xzero/executor/LinuxScheduler.h
endif

if PLATFORM_DARWIN
# XXX some stuff here, if any
endif

####### libXzeroFlow.a ########################################################

# TODO option(ENABLE_FLOW_DIRECT_THREADED_VM "Flow VM using direct threaded mode [default: off]" OFF)

lib_LIBRARIES += libXzeroFlow.a

libXzeroFlow_a_SOURCES = \
	xzero-flow/AST.cc \
	xzero-flow/ASTPrinter.cc \
	xzero-flow/FlowCallVisitor.cc \
	xzero-flow/FlowLexer.cc \
	xzero-flow/FlowLocation.cc \
	xzero-flow/FlowParser.cc \
	xzero-flow/FlowToken.cc \
	xzero-flow/FlowType.cc \
	xzero-flow/IRGenerator.cc \
	xzero-flow/TargetCodeGenerator.cc \
	xzero-flow/ir/BasicBlock.cc \
	xzero-flow/ir/Constant.cc \
	xzero-flow/ir/ConstantArray.cc \
	xzero-flow/ir/ConstantValue.cc \
	xzero-flow/ir/IRBuilder.cc \
	xzero-flow/ir/IRHandler.cc \
	xzero-flow/ir/IRProgram.cc \
	xzero-flow/ir/Instr.cc \
	xzero-flow/ir/InstructionVisitor.cc \
	xzero-flow/ir/Instructions.cc \
	xzero-flow/ir/PassManager.cc \
	xzero-flow/ir/Value.cc \
	xzero-flow/transform/EmptyBlockElimination.cc \
	xzero-flow/transform/InstructionElimination.cc \
	xzero-flow/transform/UnusedBlockPass.cc \
	xzero-flow/vm/ConstantPool.cc \
	xzero-flow/vm/Handler.cc \
	xzero-flow/vm/Instruction.cc \
	xzero-flow/vm/Match.cc \
	xzero-flow/vm/MatchClass.cc \
	xzero-flow/vm/NativeCallback.cc \
	xzero-flow/vm/Program.cc \
	xzero-flow/vm/Runner.cc \
	xzero-flow/vm/Runtime.cc \
	xzero-flow/vm/Signature.cc

libXzeroFlowdir = $(includedir)
nobase_libXzeroFlow_HEADERS = \
	xzero-flow/AST.h \
	xzero-flow/ASTPrinter.h \
	xzero-flow/ASTVisitor.h \
	xzero-flow/Api.h \
	xzero-flow/FlowCallVisitor.h \
	xzero-flow/FlowLexer.h \
	xzero-flow/FlowLocation.h \
	xzero-flow/FlowParser.h \
	xzero-flow/FlowToken.h \
	xzero-flow/FlowType.h \
	xzero-flow/IRGenerator.h \
	xzero-flow/TargetCodeGenerator.h \
	xzero-flow/ir/BasicBlock.h \
	xzero-flow/ir/Constant.h \
	xzero-flow/ir/ConstantArray.h \
	xzero-flow/ir/ConstantValue.h \
	xzero-flow/ir/HandlerPass.h \
	xzero-flow/ir/IRBuilder.h \
	xzero-flow/ir/IRBuiltinFunction.h \
	xzero-flow/ir/IRBuiltinHandler.h \
	xzero-flow/ir/IRHandler.h \
	xzero-flow/ir/IRProgram.h \
	xzero-flow/ir/Instr.h \
	xzero-flow/ir/InstructionVisitor.h \
	xzero-flow/ir/Instructions.h \
	xzero-flow/ir/PassManager.h \
	xzero-flow/ir/Value.h \
	xzero-flow/transform/EmptyBlockElimination.h \
	xzero-flow/transform/InstructionElimination.h \
	xzero-flow/transform/UnusedBlockPass.h \
	xzero-flow/vm/ConstantPool.h \
	xzero-flow/vm/Handler.h \
	xzero-flow/vm/Instruction.h \
	xzero-flow/vm/Match.h \
	xzero-flow/vm/MatchClass.h \
	xzero-flow/vm/NativeCallback.h \
	xzero-flow/vm/Params.h \
	xzero-flow/vm/Program.h \
	xzero-flow/vm/Runner.h \
	xzero-flow/vm/Runtime.h \
	xzero-flow/vm/Signature.h

####### TESTS #################################################################

TESTS += xzero_test
check_PROGRAMS += xzero_test

xzero_test_SOURCES = \
	xzero-flow/FlowLexer-test.cc \
	xzero-flow/FlowParser-test.cc \
	xzero/Base64-test.cc \
	xzero/Buffer-test.cc \
	xzero/Callback-test.cc \
	xzero/DeadlineTimer-test.cc \
	xzero/HugeBuffer-test.cc \
	xzero/MimeTypes-test.cc \
	xzero/Option-test.cc \
	xzero/raft/Generator-test.cc \
	xzero/raft/InetTransport-test.cc \
	xzero/raft/Parser-test.cc \
	xzero/raft/Server-test.cc \
	xzero/raft/ServerUtil-test.cc \
	xzero/RefPtr-test.cc \
	xzero/Result-test.cc \
	xzero/StringUtil-test.cc \
	xzero/TokenShaper-test.cc \
	xzero/Tokenizer-test.cc \
	xzero/Uri-test.cc \
	xzero/cli/CLI-test.cc \
	xzero/executor/PosixScheduler-test.cc \
	xzero/executor/ThreadPool-test.cc \
	xzero/hash/FNV-test.cc \
	xzero/http/HeaderField-test.cc \
	xzero/http/HeaderFieldList-test.cc \
	xzero/http/HttpChannel-test.cc \
	xzero/http/HttpFileHandler-test.cc \
	xzero/http/HttpRangeDef-test.cc \
	xzero/http/HttpRequestInfo-test.cc \
	xzero/http/client/HttpClient-test.cc \
	xzero/http/fastcgi/Generator-test.cc \
	xzero/http/fastcgi/RequestParser-test.cc \
	xzero/http/fastcgi/ResponseParser-test.cc \
	xzero/http/hpack/DynamicTable-test.cc \
	xzero/http/hpack/Generator-test.cc \
	xzero/http/hpack/Parser-test.cc \
	xzero/http/hpack/StaticTable-test.cc \
	xzero/http/http1/Connection-test.cc \
	xzero/http/http1/Generator-test.cc \
	xzero/http/http1/Parser-test.cc \
	xzero/http/http2/Connection-test.cc \
	xzero/http/http2/Flow-test.cc \
	xzero/http/http2/Generator-test.cc \
	xzero/http/http2/Parser-test.cc \
	xzero/http/http2/Stream-test.cc \
	xzero/io/DataChain-test.cc \
	xzero/io/MemoryFile-test.cc \
	xzero/io/MemoryMap-test.cc \
	xzero/net/ByteArrayEndPoint-test.cc \
	xzero/net/Cidr-test.cc \
	xzero/thread/Future-test.cc \
	xzero/thread/Queue-test.cc \
	xzero/util/BinaryReader-test.cc \
	xzero/util/BinaryWriter-test.cc

if PLATFORM_LINUX
xzero_test_SOURCES += \
	xzero/executor/LinuxScheduler-test.cc
endif

xzero_test_LDADD = \
	libXzeroTesting_main.a \
	libXzeroFlow.a \
	libXzero.a

####### x0d ###################################################################

bin_PROGRAMS += x0d

x0d_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	-I$(top_srcdir)/src \
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
	src/x0d/XzeroContext.cc \
	src/x0d/XzeroDaemon.cc \
	src/x0d/XzeroEventHandler.cc \
	src/x0d/XzeroModule.cc \
	src/x0d/XzeroState.cc \
	src/modules/access.cc \
	src/modules/accesslog.cc \
	src/modules/auth.cc \
	src/modules/compress.cc \
	src/modules/core.cc \
	src/modules/dirlisting.cc \
	src/modules/empty_gif.cc \
	src/modules/proxy.cc \
	src/modules/userdir.cc \
	src/modules/webdav.cc \
	src/main.cc

x0ddir = $(includedir)/x0d
x0d_HEADERS = \
	src/x0d/XzeroContext.h \
	src/x0d/XzeroDaemon.h \
	src/x0d/XzeroEventHandler.h \
	src/x0d/XzeroModule.h \
	src/x0d/XzeroState.h

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

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = XzeroBase.pc XzeroFlow.pc XzeroTest.pc

# vim:ts=8
# EOF
