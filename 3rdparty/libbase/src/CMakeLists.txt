add_definitions(-pthread)

set(libbase_SRC
  AnsiColor.cpp
  Base64.cpp
  Buffer.cpp
  Cidr.cpp
  CacheService.cpp
  Counter.cpp
  DateTime.cpp
  DebugLogger.cpp
  IniFile.cpp
  JsonWriter.cpp
  Library.cpp
  Logger.cpp
  Logging.cpp
  LogFile.cpp
  MemoryPool.cpp
  NativeSymbol.cpp
  Process.cpp
  RegExp.cpp
  ServerSocket.cpp
  Severity.cpp
  Socket.cpp
  SocketDriver.cpp
  SocketSpec.cpp
  StackTrace.cpp
  strutils.cpp
  TextFormatter.cpp
  TimeSpan.cpp
  Tokenizer.cpp
  Url.cpp

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
  io/FixedBufferSink.cpp
  io/NullFilter.cpp
  io/NullSource.cpp
  io/Pipe.cpp
  io/PipeSink.cpp
  io/PipeSource.cpp
  io/SocketSink.cpp
  io/SyslogSink.cpp
)

if(ENABLE_MYSQL)
  set(libbase_sql_SRC
    sql/SqlConnection.cpp
    sql/SqlResult.cpp
    sql/SqlStatement.cpp
  )
  add_definitions(${MYSQL_CFLAGS})
endif()

add_library(base SHARED ${libbase_SRC} ${libbase_sql_SRC})
target_link_libraries(base rt)
target_link_libraries(base dl)
target_link_libraries(base pthread)
target_link_libraries(base ${SD_LIBRARIES})
target_link_libraries(base ${ZLIB_LIBRARIES})
target_link_libraries(base ${BZIP2_LIBRARIES})
target_link_libraries(base ${PCRE_LIBRARIES})
target_link_libraries(base ${EV_LIBRARIES})

if(ENABLE_MYSQL)
  target_link_libraries(base ${MYSQL_LDFLAGS})
endif()

set_target_properties(base PROPERTIES VERSION ${LIBBASE_VERSION})
set_target_properties(base PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_BASE=1")
install(TARGETS base DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
