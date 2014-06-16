add_definitions(
  -pthread
  -DPACKAGE_NAME="${PACKAGE_NAME}"
  -DPACKAGE_VERSION="${PACKAGE_VERSION}"
  -DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
  -DSYSCONFDIR="${SYSCONFDIR}"
  -DPLUGINDIR="${PLUGINDIR}"
  -DLOGDIR="${LOGDIR}"
  -DVERSION="${PACKAGE_VERSION}")

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
  base/LogFile.cpp
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
  set(x0_sql_SRC
    sql/SqlConnection.cpp
    sql/SqlResult.cpp
    sql/SqlStatement.cpp
  )
  add_definitions(${MYSQL_CFLAGS})
endif()

add_library(XzeroBase SHARED ${x0_base_SRC} ${x0_io_SRC} ${x0_sql_SRC})
target_link_libraries(XzeroBase sd-daemon)

if(ENABLE_STATIC)
  add_library(XzeroBase_static STATIC ${x0_base_SRC} ${x0_io_SRC} ${x0_sql_SRC} sd-daemon_static)
endif()

if(APPLE)
  target_link_libraries(XzeroBase -stdlib=libc++)
  if(ENABLE_STATIC)
    target_link_libraries(XzeroBase_static -stdlib=libc++)
  endif()
else()
  target_link_libraries(XzeroBase rt)
  if(ENABLE_STATIC)
    target_link_libraries(XzeroBase_static rt)
  endif()
endif()

if(ENABLE_MYSQL)
  target_link_libraries(XzeroBase ${MYSQL_LDFLAGS})
  if(ENABLE_STATIC)
    target_link_libraries(XzeroBase_static ${MYSQL_LDFLAGS})
  endif()
endif()

set_target_properties(XzeroBase PROPERTIES VERSION ${PACKAGE_VERSION})
set_target_properties(XzeroBase PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_X0=1")
target_link_libraries(XzeroBase dl pthread ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${PCRE_LIBRARIES} ${EV_LIBRARIES})
install(TARGETS XzeroBase DESTINATION ${LIBDIR})

if(ENABLE_STATIC)
set_target_properties(XzeroBase_static PROPERTIES VERSION ${PACKAGE_VERSION})
set_target_properties(XzeroBase PROPERTIES COMPILE_FLAGS "-DBUILD_X0=1")
target_link_libraries(XzeroBase_static dl pthread ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${PCRE_LIBRARIES} ${EV_LIBRARIES})
install(TARGETS XzeroBase_static DESTINATION ${LIBDIR})
endif()
# }}}

add_subdirectory(http)
add_subdirectory(flow)
add_subdirectory(capi)
