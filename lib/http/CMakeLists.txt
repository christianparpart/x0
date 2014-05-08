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
install(TARGETS XzeroHttp DESTINATION ${LIBDIR})
