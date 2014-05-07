if(ENABLE_EXAMPLES)
  add_executable(app1 app1.cpp)
  target_link_libraries(app1 XzeroBase XzeroHttp)

  add_executable(capi-app1 capi-app1.c)
  target_link_libraries(capi-app1 XzeroBase x0capi)

  add_executable(capi-app2 capi-app2.c)
  target_link_libraries(capi-app2 XzeroBase x0capi)

  add_executable(capi-post-data capi-post-data.c)
  target_link_libraries(capi-post-data XzeroBase x0capi)

  add_executable(tcp-echo-server tcp-echo-server.cpp)
  target_link_libraries(tcp-echo-server XzeroBase)

  add_executable(tcp-echo-server-splice tcp-echo-server-splice.cpp)
  target_link_libraries(tcp-echo-server-splice XzeroBase)
endif()
