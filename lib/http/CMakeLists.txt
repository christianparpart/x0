set(x0_http_SRC
  HttpClient.cpp
  HttpConnection.cpp
  HttpFile.cpp
  HttpFileMgr.cpp
  HttpMessageParser.cpp
  HttpRequest.cpp
  HttpServer.cpp
  HttpStatus.cpp
  HttpWorker.cpp
  HttpVary.cpp
)

add_library(XzeroHttp SHARED ${x0_http_SRC})
target_link_libraries(XzeroHttp XzeroBase)
set_target_properties(XzeroHttp PROPERTIES VERSION ${PACKAGE_VERSION})
set_target_properties(XzeroHttp PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_X0=1")
install(TARGETS XzeroHttp DESTINATION ${LIBDIR})

if(ENABLE_STATIC)
add_library(XzeroHttp_static STATIC ${x0_http_SRC})
target_link_libraries(XzeroHttp_static XzeroBase_static)
set_target_properties(XzeroHttp_static PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS XzeroHttp_static DESTINATION ${LIBDIR})
endif()
