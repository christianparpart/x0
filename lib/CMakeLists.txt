add_definitions(
  -pthread
  -std=c++0x
  -DPACKAGE_NAME="${PACKAGE_NAME}"
  -DPACKAGE_VERSION="${PACKAGE_VERSION}"
  -DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
  -DSYSCONFDIR="${SYSCONFDIR}"
  -DPLUGINDIR="${PLUGINDIR}"
  -DLOGDIR="${LOGDIR}"
  -DVERSION="${PACKAGE_VERSION}")

add_definitions(-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_X0=1)

if(APPLE)
  add_definitions(-stdlib=libc++)
endif()

# {{{ XzeroBase
set(x0_base_SRC
  base/AnsiColor.cpp
  base/Base64.cpp
  base/Buffer.cpp
  base/Cidr.cpp
  base/CacheService.cpp
  base/ConstBuffer.cpp
  base/Counter.cpp
  base/DateTime.cpp
  base/DebugLogger.cpp
  base/IniFile.cpp
  base/JsonWriter.cpp
  base/Library.cpp
  base/Logger.cpp
  base/Logging.cpp
  base/MemoryPool.cpp
  base/NativeSymbol.cpp
  base/Process.cpp
  base/RegExp.cpp
  base/ServerSocket.cpp
  base/Severity.cpp
  base/Socket.cpp
  base/SocketDriver.cpp
  base/SocketSpec.cpp
  base/StackTrace.cpp
  base/strutils.cpp
  base/TextFormatter.cpp
  base/TimeSpan.cpp
  base/Tokenizer.cpp
  base/Url.cpp
)

set(x0_io_SRC
  io/BufferRefSource.cpp
  io/BufferSink.cpp
  io/BufferSource.cpp
  io/CallbackSource.cpp
  io/ChainFilter.cpp
  io/ChunkedEncoder.cpp
  io/CompositeSource.cpp
  io/CompressFilter.cpp
  io/FileSink.cpp
  io/FileSource.cpp
  io/Filter.cpp
  io/FilterSource.cpp
  io/NullFilter.cpp
  io/NullSource.cpp
  io/Pipe.cpp
  io/PipeSink.cpp
  io/PipeSource.cpp
  io/SocketSink.cpp
  io/SyslogSink.cpp
)

if(ENABLE_MYSQL)
  set(x0_sql_SRC
    sql/SqlConnection.cpp
    sql/SqlResult.cpp
    sql/SqlStatement.cpp
  )
  add_definitions(${MYSQL_CFLAGS})
endif()

add_library(XzeroBase SHARED ${x0_base_SRC} ${x0_io_SRC} ${x0_sql_SRC})

if(APPLE)
  target_link_libraries(XzeroBase -stdlib=libc++)
endif()

if(ENABLE_MYSQL)
  target_link_libraries(XzeroBase ${MYSQL_LDFLAGS})
endif()

target_link_libraries(XzeroBase dl pthread ${SD_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${PCRE_LIBRARIES} ${EV_LIBRARIES})
set_target_properties(XzeroBase PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS XzeroBase DESTINATION ${LIBDIR})
# }}}

# XzeroHttp
set(x0_http_SRC
  http/HttpClient.cpp
  http/HttpConnection.cpp
  http/HttpFile.cpp
  http/HttpFileMgr.cpp
  http/HttpMessageParser.cpp
  http/HttpRequest.cpp
  http/HttpServer.cpp
  http/HttpStatus.cpp
  http/HttpWorker.cpp
  http/HttpVary.cpp
)
add_library(XzeroHttp SHARED ${x0_http_SRC})
target_link_libraries(XzeroHttp XzeroBase)
set_target_properties(XzeroHttp PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS XzeroHttp DESTINATION ${LIBDIR})

if(ENABLE_XZERO_FLOW)
  # XzeroFlow
  set(x0_flow_SRC
    flow/AST.cpp
    flow/ASTPrinter.cpp
    flow/FlowCallVisitor.cpp
    flow/FlowLexer.cpp
    flow/FlowLocation.cpp
    flow/FlowParser.cpp
    flow/FlowToken.cpp
    flow/FlowType.cpp
    flow/IRGenerator.cpp
    flow/TargetCodeGenerator.cpp

    flow/ir/BasicBlock.cpp
    flow/ir/Constant.cpp
    flow/ir/ConstantValue.cpp
    flow/ir/Instr.cpp
    flow/ir/Instructions.cpp
    flow/ir/InstructionVisitor.cpp
    flow/ir/IRBuilder.cpp
    flow/ir/IRHandler.cpp
    flow/ir/IRProgram.cpp
    flow/ir/PassManager.cpp
    flow/ir/Value.cpp

    flow/transform/EmptyBlockElimination.cpp
    flow/transform/InstructionElimination.cpp

    flow/vm/Handler.cpp
    flow/vm/Instruction.cpp
    flow/vm/Match.cpp
    flow/vm/MatchClass.cpp
    flow/vm/NativeCallback.cpp
    flow/vm/Program.cpp
    flow/vm/Runner.cpp
    flow/vm/Runtime.cpp
    flow/vm/Signature.cpp
  )
  add_library(XzeroFlow SHARED ${x0_flow_SRC})
  set_target_properties(XzeroFlow PROPERTIES LINK_FLAGS "${LDFLAGS}")
  target_link_libraries(XzeroFlow LINK_PRIVATE XzeroBase dl pthread)
  set_target_properties(XzeroFlow PROPERTIES VERSION ${PACKAGE_VERSION})
  install(TARGETS XzeroFlow DESTINATION ${LIBDIR})
endif()

# Xzero C API
add_library(x0capi SHARED
  capi/http.cpp
  capi/inifile.cpp
)
target_link_libraries(x0capi XzeroHttp XzeroBase)
set_target_properties(x0capi PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS x0capi DESTINATION ${LIBDIR})
